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

#include <types.h>
#include "mmu.h"
#include "reg.h"

#include <assert.h>

/*
 *  Quotes from the manual:
 *
 *      64-entry, fully-associative unified (for instruction and data accesses) L2 TLB array (TLB1)
 *      supports the 11 VSP page sizes shown in Section 6.2.3, “Variable-Sized Pages.”
 *
 * TODO: document parameters
 */
void pok_ppc_tlb_write(
        unsigned tlbsel,
        uint32_t virtual, 
        uint64_t physical, 
        unsigned pgsize_enum, 
        unsigned permissions,
        unsigned wimge,
        unsigned pid,
        unsigned entry,
        pok_bool_t  valid
        )
{
    /*
     * TLB1 can be written by first writing the necessary information into MAS0–MAS3, MAS5, MAS7, and
     * MAS8 using mtspr and then executing the tlbwe instruction. To write an entry into TLB1,
     * MAS0[TLBSEL] must be equal to 1, and MAS0[ESEL] must point to the desired entry. When the tlbwe
     * instruction is executed, the TLB entry information stored in MAS0–MAS3, MAS5, MAS7, and MAS8 is
     * written into the selected TLB entry in the TLB1 array.
     */

    uint32_t mas0, mas1, mas2, mas3, mas7;

    assert(tlbsel <= 1) ;

    mas0 = MAS0_TLBSEL(tlbsel) | MAS0_ESEL(entry);
    mas1 = ((valid != 0)? MAS1_VALID : 0) | MAS1_TID(pid) | MAS1_TSIZE(pgsize_enum);
    mas2 = (virtual & MAS2_EPN) | wimge;
    mas3 = (physical & MAS3_RPN) | permissions; 
    mas7 = physical >> 32;

    mtspr(SPRN_MAS0, mas0); 
    mtspr(SPRN_MAS1, mas1); 
    mtspr(SPRN_MAS2, mas2);
    mtspr(SPRN_MAS3, mas3);
    mtspr(SPRN_MAS7, mas7);

    asm volatile("isync; tlbwe; isync":::"memory");
}

void pok_ppc_tlb_clear_entry(
        unsigned tlbsel,
        unsigned entry
    ) {
    //pok_ppc_tlb_write(tlbsel,
        //0, 0,
        //0,
        //0, 0, 0,
        //entry,
        //FALSE);
    uint32_t mas0, mas1, mas2, mas3, mas7;

    assert(tlbsel <= 1) ;

    mas0 = MAS0_TLBSEL(tlbsel) | MAS0_ESEL(entry);
    mas1 = 0;
    mas2 = 0;
    mas3 = 0;
    mas7 = 0;

    mtspr(SPRN_MAS0, mas0); 
    mtspr(SPRN_MAS1, mas1); 
    mtspr(SPRN_MAS2, mas2);
    mtspr(SPRN_MAS3, mas3);
    mtspr(SPRN_MAS7, mas7);

    asm volatile("isync; msync; tlbwe; isync":::"memory");

}
/*
unsigned pok_ppc_get_tlb_nentry(unsigned tlbsel) {
    static unsigned regid[] =  { SPRN_TLB0CFG,
        SPRN_TLB1CFG, 
        SPRN_TLB2CFG, 
        SPRN_TLB3CFG 
    };

    assert(tlbsel < 5);
    unsigned sprn = regid[tlbsel];
    return mfspr(sprn) & TLBnCFG_N_ENTRY_MASK;
}
*/

void pok_ppc_tlb_read_entry(
        unsigned tlbsel,
        unsigned entry,
        unsigned *valid, 
        unsigned *tsize, 
        uint32_t *epn,
        uint64_t *rpn)
{
    uint32_t mas0, mas1;

    assert(tlbsel <= 1) ;

    mas0 = MAS0_TLBSEL(tlbsel) | MAS0_ESEL(entry);

    mtspr(SPRN_MAS0, mas0);
    asm volatile("tlbre;isync");
    mas1 = mfspr(SPRN_MAS1);

    *valid = (mas1 & MAS1_VALID);
    *tsize = (mas1 >> 7) & 0x1f;
    *epn = mfspr(SPRN_MAS2) & MAS2_EPN;
    *rpn = mfspr(SPRN_MAS3) & MAS3_RPN;
    *rpn |= ((uint64_t)mfspr(SPRN_MAS7)) << 32;
}


static inline const char* pok_ppc_tlb_size(unsigned size)
{
    switch (size) {
#define CASE(x) case E500MC_PGSIZE_##x: return #x; 
        CASE(4K);
        CASE(16K);
        CASE(64K);
        CASE(256K);
        CASE(1M);
        CASE(4M);
        CASE(16M);
        CASE(64M);
        CASE(256M);
        CASE(1G);
        CASE(4G);
#undef CASE
        default:
        return "Unknown";
    }
}

void pok_ppc_tlb_print(unsigned tlbsel)
{
    unsigned limit = pok_ppc_tlb_get_nentry(1);

    for (unsigned i = 0; i < limit; i++) {
        unsigned valid;
        unsigned tsize;
        uint32_t epn;
        uint64_t rpn;
        pok_ppc_tlb_read_entry(tlbsel, i,
                &valid,
                &tsize,
                &epn,
                &rpn
                );
        if (valid) {
            printf("DEBUG: tlb entry %d:%d:\r\n", tlbsel, i);
            printf("DEBUG:   Valid\r\n");
            printf("DEBUG:   Effective: %p\r\n", (void*)epn);
            // FIXME This is wrong. We print only 32 bits out of 36
            printf("DEBUG:   Physical: %x:%p\r\n",
                    (unsigned)(rpn>>32), (void*)(unsigned)rpn);
            printf("DEBUG:   Size: %s\r\n", pok_ppc_tlb_size(tsize));
        }
    }
}
