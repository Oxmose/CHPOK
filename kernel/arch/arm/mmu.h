/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2017 ISPRAS
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

#ifndef __ARM_MMU_H__
#define __ARM_MMU_H__

#define L1_TYPE_FAULT (0)
#define L1_TYPE_TABLE (1)
#define L1_TYPE_SECT  (2)
#define L1_TYPE_MASK  (3)

#define L1_SECT_B      ( 1 << 2)   // bufferable
#define L1_SECT_C      ( 1 << 3)   // cacheable
#define L1_SECT_XN     ( 1 << 4)   // execute never
#define L1_SECT_AP(x)  ((x) << 10) // access permission
#define L1_SECT_TEX(x) ((x) << 12) // type extension
#define L1_SECT_APX    ( 1 << 15)
#define L1_SECT_S      ( 1 << 16)  // shareable
#define L1_SECT_nG     ( 1 << 17)  // non global
#define L1_SECT_SUPER  ( 1 << 18)  // supersection

#define L1_SECT_PRIVILEGED_RW (L1_SECT_AP(1))
#define L1_SECT_MEM_NORMAL_CACHEABLE (L1_SECT_TEX(0) | L1_SECT_C | L1_SECT_B)

#define L1_SECT_MEM_DEVICE (L1_SECT_TEX(2))

#endif