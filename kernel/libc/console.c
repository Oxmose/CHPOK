#include <libc.h>
#include <cons.h>


/*
*   Function described in cons.c:
*   data_to_read_0() - return 1 if there is data to read, or 0 otherwise
*   read_serial_0()  - read and return 1 word from receiver buffer
*/


static int
serial_proc_data(void) //Read data from DUART (1 word)
{
    if (!data_to_read_0())
        return -1;
    return read_serial_0();
}


#define CONSBUFSIZE 512

static struct {
    uint8_t buf[CONSBUFSIZE];
    uint32_t rpos;
    uint32_t wpos;
} cons;

static void
cons_intr() //read all elements from DUART
{
    int c;

    while ((c = serial_proc_data()) != -1) {
        if (c == 0)
            continue;
        cons.buf[cons.wpos++] = c;
        if (cons.wpos == CONSBUFSIZE)
            cons.wpos = 0;
    }
}

// return the next input character from the console, or 0 if none waiting
int
cons_getc(void) //Get 1 element from cons
{
    int c;

    cons_intr();

    // grab the next character from the input buffer.
    if (cons.rpos != cons.wpos) {
        c = cons.buf[cons.rpos++];
        if (cons.rpos == CONSBUFSIZE)
            cons.rpos = 0;
        return c;
    }
    return 0;
}


int
getchar(void)
{
    int c;
    
    while ((c = cons_getc()) == 0){
//#ifdef POK_ARCH_X86
#ifdef __i386__
        asm("hlt");
#endif
    }
    return c;
}

