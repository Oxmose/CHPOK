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

#ifndef __POK_SYS_CHANNEL_DRIVER_H__
#define __POK_SYS_CHANNEL_DRIVER_H__

#include <net/network.h>
typedef struct channel_driver {
    pok_bool_t (*send)(
        char *payload,
        size_t payload_size,
        size_t max_backstep,
        void *driver_data
    );

    void (*register_received_callback)(
            pok_bool_t (*callback)(
                uint32_t ip,
                uint16_t port,
                const char *payload,
                size_t length
                )
            );

    /* will call callbacks on received packets */
    void (*receive)();

    /* We finished sending portion, so driver MAY flush send buffers */
    void (*flush_send)();


} channel_driver_t;
#endif
