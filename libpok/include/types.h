/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 */


#ifndef __POK_TYPES_H__
#define __POK_TYPES_H__

#include <stdint.h>
#include <stddef.h>

#include <errno.h>

#define FALSE  0
#define TRUE   1
#define bool_t int
#define pok_bool_t int

typedef uint32_t  pok_port_size_t;
typedef uint8_t   pok_port_direction_t;
typedef uint8_t   pok_port_id_t;
typedef uint32_t  pok_size_t;
typedef uint8_t   pok_range_t;
typedef uint8_t   pok_buffer_id_t;
typedef uint8_t   pok_blackboard_id_t;
typedef uint8_t   pok_mutex_id_t;
typedef uint16_t  pok_event_id_t;
typedef uint16_t  pok_sem_id_t;
typedef uint8_t   pok_partition_id_t;
typedef uint8_t   pok_thread_id_t;

typedef enum {
    POK_BLACKBOARD_EMPTY,
    POK_BLACKBOARD_OCCUPIED
} pok_blackboard_empty_t;
typedef uint16_t pok_sem_value_t;
typedef enum {
    POK_EVENT_UP,
    POK_EVENT_DOWN
} pok_event_state_t;

typedef uint16_t pok_message_size_t; // 0....8192
typedef uint16_t pok_message_range_t; // 0...512


typedef int64_t  pok_time_t;

typedef enum {
    POK_QUEUING_DISCIPLINE_FIFO,
    POK_QUEUING_DISCIPLINE_PRIORITY,
} pok_queuing_discipline_t;

#define MAX_NAME_LENGTH 30

#endif
