/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/arp/config.yaml).
 */
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

#ifndef __ARP_ANSWERER_GEN_H__
#define __ARP_ANSWERER_GEN_H__


    #include <interfaces/message_handler_gen.h>

    #include <interfaces/ethernet_packet_sender_gen.h>

typedef struct ARP_ANSWERER_state {
    uint8_t src_mac[6];
    uint32_t good_ips_len;
    uint32_t good_ips[10];
}ARP_ANSWERER_state;

typedef struct {
    ARP_ANSWERER_state state;
    struct {
            struct {
                message_handler ops;
            } portA;
    } in;
    struct {
            struct {
                ethernet_packet_sender *ops;
                self_t *owner;
            } portB;
    } out;
} ARP_ANSWERER;



      ret_t arp_receive(ARP_ANSWERER *, const char *, size_t);

      ret_t ARP_ANSWERER_call_portB_mac_send(ARP_ANSWERER *, char *, size_t, size_t, uint8_t *, enum ethertype);
      ret_t ARP_ANSWERER_call_portB_flush(ARP_ANSWERER *);






#endif
