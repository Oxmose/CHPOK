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
 *
 * Some defines was taken from linux source "include/uapi/linux/pci_regs.h"
 * PCI standard defines
 * Copyright 1994, Drew Eckhardt
 * Copyright 1997--1999 Martin Mares <mj@ucw.cz>
 *
 */


# ifndef __POK__PCI_H__
# define __POK__PCI_H__

#include <types.h>
#include <ioports.h>

/*
 * PCI configuration registers
 */
#  define PCI_CONFIG_ADDRESS	0xCF8
#  define PCI_CONFIG_DATA	0xCFC

/*
 * Configuration space registers
 */
#define PCI_VENDOR_ID           0x00    /* 16 bits */
#define PCI_DEVICE_ID           0x02    /* 16 bits */
#define PCI_COMMAND             0x04    /* 16 bits */
#define PCI_CLASS_REVISION      0x08    /* High 24 bits are class, low 8 revision */
#define PCI_REVISION_ID         0x08    /* Revision ID */
#define PCI_CLASS_PROG          0x09    /* Reg. Level Programming Interface */
#define PCI_CLASS_DEVICE        0x0a    /* Device class */
#define PCI_HEADER_TYPE         0x0E
#define  PCI_HEADER_TYPE_NORMAL         0
#define  PCI_HEADER_TYPE_BRIDGE         1
#define  PCI_HEADER_TYPE_CARDBUS        2

/*
 * Base addresses specify locations in memory or I/O space.
 */
#define PCI_BASE_ADDRESS_0      0x10    /* 32 bits */
#define PCI_BASE_ADDRESS_1      0x14    /* 32 bits [htype 0,1 only] */
#define PCI_BASE_ADDRESS_2      0x18    /* 32 bits [htype 0 only] */
#define PCI_BASE_ADDRESS_3      0x1c    /* 32 bits */
#define PCI_BASE_ADDRESS_4      0x20    /* 32 bits */
#define PCI_BASE_ADDRESS_5      0x24    /* 32 bits */

#define  PCI_BASE_ADDRESS_SPACE         0x01    /* 0 = memory, 1 = I/O */
#define  PCI_BASE_ADDRESS_SPACE_IO      0x01
#define  PCI_BASE_ADDRESS_SPACE_MEMORY  0x00
#define  PCI_BASE_ADDRESS_MEM_TYPE_MASK 0x06
#define  PCI_BASE_ADDRESS_MEM_TYPE_32   0x00    /* 32 bit address */
#define  PCI_BASE_ADDRESS_MEM_TYPE_1M   0x02    /* Below 1M [obsolete] */
#define  PCI_BASE_ADDRESS_MEM_TYPE_64   0x04    /* 64 bit address */
#define  PCI_BASE_ADDRESS_MEM_PREFETCH  0x08    /* prefetchable? */
#define  PCI_BASE_ADDRESS_MEM_MASK      (~0x0fUL)
#define  PCI_BASE_ADDRESS_IO_MASK       (~0x03UL)

#define PCI_ROM_ADDRESS         0x30    /* Bits 31..11 are address, 10..1 reserved */
#define  PCI_ROM_ADDRESS_ENABLE 0x01
#define PCI_ROM_ADDRESS_MASK    (~0x7ffUL)

/*
 * Useful defines...
 */
#  define PCI_BUS_MAX		8
#  define PCI_DEV_MAX		32
#  define PCI_FN_MAX		8

/* Command regs*/
#define  PCI_COMMAND_IO           0x1     /* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY       0x2     /* Enable response in Memory space */
#define  PCI_COMMAND_MASTER       0x4     /* Enable bus mastering */
#define  PCI_COMMAND_SPECIAL      0x8     /* Enable response to special cycles */
#define  PCI_COMMAND_INVALIDATE   0x10    /* Use memory write and invalidate */
#define  PCI_COMMAND_VGA_PALETTE  0x20   /* Enable palette snooping */
#define  PCI_COMMAND_PARITY       0x40    /* Enable parity checking */
#define  PCI_COMMAND_WAIT         0x80    /* Enable address/data stepping */
#define  PCI_COMMAND_SERR         0x100   /* Enable SERR */
#define  PCI_COMMAND_FAST_BACK    0x200   /* Enable back-to-back writes */
#define  PCI_COMMAND_INTX_DISABLE 0x400 /* INTx Emulation Disable */

/*
 * For pci devices resources numbers are
 *  0-5  BAR (base address)
 *  6  expansion ROM
 */
#define PCI_NUM_RESOURCES 7
enum PCI_RESOURCE_INDEX {
    PCI_RESOURCE_BAR0,
    PCI_RESOURCE_BAR1,
    PCI_RESOURCE_BAR2,
    PCI_RESOURCE_BAR3,
    PCI_RESOURCE_BAR4,
    PCI_RESOURCE_BAR5,
    PCI_RESOURCE_ROM
};

enum PCI_RESOURCE_TYPE {
  PCI_RESOURCE_TYPE_BAR_IO,
  PCI_RESOURCE_TYPE_BAR_MEM,
  PCI_RESOURCE_TYPE_ROM
};

enum PCI_RESOURCE_MEM_FLAG_MASKS{
    /* if this bit is
     *      0 then resource is 64bit region //UNSUPPORTED yet!!
     *      1 then resource is 32bit region
     */
    PCI_RESOURCE_MEM_MASK_32 = 1,

    /* if this bit is
     *      0 then resource has prefetchable memory region
     *      1 then resource has non-prefetchable memory region
     */
    PCI_RESOURCE_MEM_MASK_PREFETCH = 2,
};

/* size == 0  means that BAR (ROM) is not enabled/supported by device
 * size != 0 && addr = 0  means that BAR (ROM) is not initialized.
 */
struct pci_resource {
    uintptr_t addr; //virtual address
    uint64_t  pci_addr; //"physical" addr in pci address space
    size_t size;
    enum PCI_RESOURCE_TYPE type;

    //sets only when type == PCI_RESOURCE_BAR_MEM.
    //For values meaning see enum PCI_RESOURCE_MEM_FLAG_MASKS
    uint8_t mem_flags;
};


/*
 * Structure to holds some device information
 */
typedef struct pci_dev
{
    uint16_t    bus;
    uint16_t    dev;
    uint16_t    fn;
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint16_t    class_code; //class code and sub class
    uint8_t     hdr_type;
    struct pci_resource resources[PCI_NUM_RESOURCES];
} s_pci_dev;

void pci_init(void);
void pci_list(void);

//result == 0 means no error
int pci_read_config_byte(struct pci_dev *dev, int where, uint8_t *val);
int pci_read_config_word(struct pci_dev *dev, int where, uint16_t *val);
int pci_read_config_dword(struct pci_dev *dev, int where, uint32_t *val);
int pci_write_config_byte(struct pci_dev *dev, int where, uint8_t val);
int pci_write_config_word(struct pci_dev *dev, int where, uint16_t val);
int pci_write_config_dword(struct pci_dev *dev, int where, uint32_t val);


/*
 * Finds pci device by triple (bus, dev, fn) and fill info
 * about the device to struct pointed by pci_dev.
 * 'resources' field is not filled for pci bridges
 *
 *  pci_dev->vendor_id == 0xFFFF means that device does not exist
 */
void pci_get_dev_by_bdf(uint8_t bus, uint8_t dev, uint8_t fn, struct pci_dev *pci_dev);

//convert legacy port to mem address that can be used in ioread/iowrite functions
uintptr_t pci_convert_legacy_port(struct pci_dev *dev, uint16_t port);

# endif /* __POK_PCI_H__ */
