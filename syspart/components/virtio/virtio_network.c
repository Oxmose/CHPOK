/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 */

#include <string.h>
#include <stdio.h>
#include <pci.h>
#include <ioports.h>

#include <smalloc.h>

#include "virtio_config.h"
#include "virtio_ids.h"
#include "virtio_pci.h"
#include "virtio_ring.h"
#include "virtio_virtqueue.h"
#include "virtio_net.h"
#include "virtio_network_device.h"

#include "VIRTIO_NET_DEV_gen.h"

#include <arinc653/process.h>
#include <memblocks.h>
#include <smalloc.h>
#include <stdlib.h>
#include <paddr.h>

#define VIRTIO_PCI_VENDORID 0x1AF4

#define VIRTIO_NETWORK_RX_VIRTQUEUE 0
#define VIRTIO_NETWORK_TX_VIRTQUEUE 1

#define PRINTF(fmt, ...) printf("VIRTIO_NET_DEV: " fmt, ##__VA_ARGS__)

static void reclaim_send_buffers(struct virtio_network_device *info);

static void lock_preemption()
{
    LOCK_LEVEL_TYPE LOCK_LEVEL;
    RETURN_CODE_TYPE ret_code;
    LOCK_PREEMPTION(&LOCK_LEVEL, &ret_code);
    if (ret_code != NO_ERROR)
        PRINTF("error in LOCK_PREEMPTION %d\n", ret_code);
}


static void unlock_preemption()
{
    LOCK_LEVEL_TYPE LOCK_LEVEL;
    RETURN_CODE_TYPE ret_code;
    UNLOCK_PREEMPTION(&LOCK_LEVEL, &ret_code);
    if (ret_code != NO_ERROR)
        PRINTF("error in UNLOCK_PREEMPTION %d\n", ret_code);

}

static pok_bool_t setup_virtqueue(
        struct virtio_network_device *dev,
        int16_t index,
        struct virtio_virtqueue *vq)
{
    // queue selector
    outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_SEL, index);

    // get queue size

    uint16_t queue_size = inw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_NUM);
    if (queue_size < 1) {
        PRINTF("wrong queue size\n");
        return FALSE;
    }

    // allocate memory and fill in vq fields
    void *mem = virtio_virtqueue_setup(&dev->allocator, vq, queue_size, VIRTIO_PCI_VRING_ALIGN);

    // give device queue's physical address
    uint64_t phys_addr = jet_virt_to_phys(&dev->heapmb, mem);

    /*
    if (phys_addr == 0) {
        PRINTF("%s: jet_virt_to_phys: virtual address is wrong\n", __func__);
        return FALSE;
    }
    */
    if (phys_addr > 0xffffffff) { //greater than 4G
        PRINTF("%s: phys_addrs greater than 4G are not supported\n", __func__);
        return FALSE;
    }

    uint32_t phys_addr_32bit = (uint32_t) phys_addr;

    outl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_PFN, phys_addr_32bit / VIRTIO_PCI_VRING_ALIGN);
    return TRUE;

}

static void set_status_bit(s_pci_dev *pcidev, uint8_t bit)
{
    bit |= inb(pcidev->resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS);
    outb(pcidev->resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS, bit);
}

static void read_mac_address(struct virtio_network_device *dev)
{
    uint8_t *mac = dev->mac;

    int i;
    for (i = 0; i < ETH_ALEN; i++) {
        mac[i] = inb(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_CONFIG_OFF(FALSE) + i);
    }
}

static void use_receive_buffer(struct virtio_network_device *dev, struct receive_buffer *buf)
{
    struct virtio_virtqueue *vq = &dev->rx_vq;

    if (vq->num_free < 1) {
        PRINTF("no free RX descriptors\n");
        return; // FIXME return error code
    }

    vq->num_free--;

    struct vring_desc *desc;
    uint16_t head = vq->free_index;

    desc = &vq->vring.desc[head];
    vq->free_index = desc->next;

    desc->addr = jet_virt_to_phys(&dev->heapmb, buf);
    /*
    if (desc->addr == 0) {
        PRINTF("%s: jet_virt_to_phys: virtual address is wrong\n", __func__);
        return;
    }
    */
    desc->len = sizeof(*buf);
    desc->flags = VRING_DESC_F_WRITE;

    int avail = vq->vring.avail->idx & (vq->vring.num-1); // wrap around
    vq->vring.avail->ring[avail] = head;

    __sync_synchronize();

    vq->vring.avail->idx++;
}

// must be called after one or more receive buffers has been added to rx avail. ring
static void notify_receive_buffers(struct virtio_network_device *dev)
{
    outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_NOTIFY, VIRTIO_NETWORK_RX_VIRTQUEUE);
}

static void setup_receive_buffers(struct virtio_network_device *dev)
{
    for (unsigned i = 0; i < dev->rx_vq.vring.num; i++) {
        // this pushes buffer to avail ring
        use_receive_buffer(dev, &dev->receive_buffers[i]);
    }
    notify_receive_buffers(dev);
}

ret_t send_frame(VIRTIO_NET_DEV * self,
        char *buffer,
        const size_t size,
        const size_t prepend_max_size,
        const size_t append_max_size)
{
    if (!self->state.info.inited)
        return EINVAL; //FIXME

    if (prepend_max_size != 0)
        return EINVAL;

    if (append_max_size != 0)
        return EINVAL;

    struct vring_desc *desc;

    struct virtio_network_device *dev = &self->state.info;

    //XXX need carefully think when do we need to call this reclaim func
    reclaim_send_buffers(dev);

    // now, send it to the virtqueue
    struct virtio_virtqueue *vq = &dev->tx_vq;
    if (vq->num_free < 1) {
        PRINTF("no free TX descriptors\n");
        return EINVAL;
    }

    //Just in case zero virtio_net_hdr fields
    memset(dev->nethdr_ptr, 0, sizeof(*dev->nethdr_ptr));

    vq->num_free -= 2; //we use 2 desc. One for virtio_net_hdr, the other one for the message

    uint16_t head = vq->free_index;
    /* Setup first descriptor as virtio_net_hdr */
    desc = &vq->vring.desc[head];
    //TODO This can be optimized by do virt_to_phys once and remembering it's result
    desc->addr = jet_virt_to_phys(&dev->heapmb, dev->nethdr_ptr);
    desc->len = sizeof(*dev->nethdr_ptr);
    desc->flags = VRING_DESC_F_NEXT;


    memcpy(dev->send_buffers[head].data, buffer, size);

    desc = &vq->vring.desc[desc->next];
    desc->addr = jet_virt_to_phys(&dev->heapmb, dev->send_buffers[head].data);
    /*
    if (desc->addr == 0) {
        PRINTF("%s: jet_virt_to_phys kvirtual address is wrong\n", __func__);
        return EINVAL;
    }
    */
    desc->len = size;
    desc->flags = VRING_DESC_F_NEXT;

    desc->flags = 0;
    vq->free_index = desc->next;

    int avail = vq->vring.avail->idx & (vq->vring.num-1); // wrap around
    vq->vring.avail->ring[avail] = head;

    __sync_synchronize();

    vq->vring.avail->idx++;

    return EOK;
}

static void reclaim_send_buffers(struct virtio_network_device *info)
{
    struct virtio_virtqueue *vq = &(info->tx_vq);

    // this function can be called by any thread
    // callbacks don't do much work, so we can run them all
    // in single critical section without worrying too much

    lock_preemption();
    while (vq->last_seen_used != vq->vring.used->idx) {
        uint16_t index = vq->last_seen_used & (vq->vring.num-1);
        struct vring_used_elem *e = &vq->vring.used->ring[index];
        struct vring_desc *head = &vq->vring.desc[e->id];
        struct vring_desc *tail = head;

        // reclaim descriptor
        uint16_t total_descriptors = 1;
        while (tail->flags & VRING_DESC_F_NEXT) {
            total_descriptors++;;
            tail = &vq->vring.desc[tail->next];
        }

        vq->num_free += total_descriptors;

        // insert chain in the beginning of the free desc. list
        tail->next = vq->free_index;
        vq->free_index = e->id; // id of head

        vq->last_seen_used++;
    }

    unlock_preemption();
}

static void reclaim_receive_buffers(VIRTIO_NET_DEV *self)
{
    struct virtio_network_device *dev = &self->state.info;
    struct virtio_virtqueue *vq = &dev->rx_vq;

    lock_preemption();

    uint16_t old_last_seen_used = vq->last_seen_used;
    while (vq->last_seen_used != vq->vring.used->idx) {
        uint16_t index = vq->last_seen_used & (vq->vring.num-1);
        struct vring_used_elem *e = &vq->vring.used->ring[index];
        struct vring_desc *desc = &vq->vring.desc[e->id];

        struct receive_buffer *buf = jet_phys_to_virt(&dev->heapmb, desc->addr);
        /*
        if (buf == 0) {
            PRINTF("%s: jet_phys_to_virt physical address is wrong\n", __func__);
            unlock_preemption();
            return;
        }
        */
        VIRTIO_NET_DEV_call_portB_handle(self, (const uint8_t *)&buf->packet, e->len - sizeof(struct virtio_net_hdr));

        // reclaim descriptor
        // FIXME support chained descriptors as well
        vq->num_free++;
        desc->next = vq->free_index;
        vq->free_index = e->id;

        vq->last_seen_used++;

        // reclaim buffer
        // i.e. push it back to avail. ring
        use_receive_buffer(dev, buf);

        // preemption point
        unlock_preemption();
        lock_preemption();
    }

    if (old_last_seen_used != vq->last_seen_used) {
        // this means we moved at least one buffer from
        // used to avail ring
        notify_receive_buffers(dev);
    }

    unlock_preemption();
}

ret_t flush_send(VIRTIO_NET_DEV *self)
{
    if (!self->state.info.inited)
        return EINVAL; //FIXME

    struct virtio_network_device *dev = &self->state.info;

    outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_NOTIFY, VIRTIO_NETWORK_TX_VIRTQUEUE);
    return EOK;
}


/*
 * PCI part
 */

static pok_bool_t init_device(VIRTIO_NET_DEV_state *state)
{
    struct virtio_network_device *dev = &state->info;

    pci_get_dev_by_bdf(state->pci_bus,
            state->pci_dev,
            state->pci_fn,
            &dev->pci_device);
    if (dev->pci_device.vendor_id == 0xFFFF) {
        PRINTF("have not found virtio device\n");
        return FALSE;
    }

    dev->pci_device.resources[PCI_RESOURCE_BAR0].addr &= ~0xFU;

    //subsystem = pci_read_reg(dev, PCI_REG_SUBSYSTEM) >> 16;
    //if (subsystem != VIRTIO_ID_NET)
    //    printf("WARNING: wrong subsystem in virtio net device");

    // 1. Reset the device
    outb(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS, 0x0);

    // 2. ACK status bit
    set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_ACKNOWLEDGE);

    // 3. DRIVER status bit
    set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_DRIVER);

    // 4. Device-specific setup
    if (!setup_virtqueue(dev, VIRTIO_NETWORK_RX_VIRTQUEUE, &dev->rx_vq)
        || !setup_virtqueue(dev, VIRTIO_NETWORK_TX_VIRTQUEUE, &dev->tx_vq))
        return FALSE;

    //pok_bsp_irq_register(virtio_network_device.pci_device.irq_line, virtio_interrupt_handler);

    // 5. Device feature bits

    uint32_t features = inl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_HOST_FEATURES);
    uint32_t recognized_features = 0;

    if (features & (1 << VIRTIO_NET_F_MAC)) {
        read_mac_address(dev);
        recognized_features |= (1 << VIRTIO_NET_F_MAC);
    } else {
        PRINTF("MAC address is not configured\n");
        set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_FAILED);
        return FALSE;
    }

    outl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_GUEST_FEATURES, recognized_features);

    // 6. DRIVER_OK status bit
    set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_DRIVER_OK);

    // 7. buffers allocation
    dev->send_buffers = jet_sallocator_alloc_array(&dev->allocator,
            sizeof(*dev->send_buffers),
            dev->tx_vq.vring.num);
    if (dev->send_buffers == NULL) {
        PRINTF("heap alloc return zero (not enough memory)\n");
        return FALSE;
    }

    dev->receive_buffers = jet_sallocator_alloc_array(&dev->allocator,
            sizeof(*dev->receive_buffers),
            dev->rx_vq.vring.num);
    if (dev->receive_buffers == NULL) {
        PRINTF("heap alloc return zero (not enough memory)\n");
        return FALSE;
    }

    setup_receive_buffers(dev);


    dev->nethdr_ptr = jet_sallocator_alloc(&dev->allocator,
            sizeof(*dev->nethdr_ptr));
    if (dev->nethdr_ptr  == NULL) {
        PRINTF("heap alloc return zero (not enough memory)\n");
        return FALSE;
    }

    return TRUE;
}

void virtio_receive_activity(VIRTIO_NET_DEV *self)
{
    if (self->state.info.inited)
        reclaim_receive_buffers(self);
}

/*
 * init
 */

void virtio_init(VIRTIO_NET_DEV *self)
{
    pok_ret_t ret = VIRTIO_NET_DEV_get_memory_block_status(self, "Heap", &self->state.info.heapmb);
    if(ret != POK_ERRNO_OK) {
        PRINTF("ERROR: Memory block for heap is not created.\n");
        abort();
    }

    jet_sallocator_init_from_memblock(&self->state.info.allocator, &self->state.info.heapmb);


    if (init_device(&self->state))
        self->state.info.inited = 1;
}
