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

#ifndef __POK_ARCH_UACCESS_H__
#define __POK_ARCH_UACCESS_H__

// TODO: Revisit (see also pok_space_* code in arch.h)
static inline pok_bool_t arch_check_access(const void* __user addr, size_t size)
{
    unsigned long p = (unsigned long)addr;
    
    return (p >= 0x80000000) && ((p + size) < (0x80000000 + 0x1000000ULL));
}

static inline pok_bool_t arch_check_access_read(const void* __user addr, size_t size)
{
     return arch_check_access(addr, size);
}


static inline pok_bool_t arch_check_access_write(void* __user addr, size_t size)
{
     return arch_check_access(addr, size);
}

#endif /* __POK_ARCH_UACCESS_H__ */
