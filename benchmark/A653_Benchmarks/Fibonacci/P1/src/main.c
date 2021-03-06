/*******************************************************************************
 * ARINC-653 RTOS Benchmark
 *
 * Developped/Ported by:
 *    Felipe Gohring de Magalhaes
 *    Jean-Baptiste Lefoul
 *    Alexy Torres Aurora Dugo
 *
 *
 * Test: Summing the Fibonacci series
 *
 * Ported from:
 *     SNU-RT Benchmarks (http://www.cprover.org/goto-cc/examples/snu.html)
 *
 ******************************************************************************/

#include "../../../BenchmarksTools/benc_config.h"

/*******************************************************************************
 * TESTS SETTINGS
 ******************************************************************************/
#define FIB_VALUE 300000

/*******************************************************************************
 * PARTITION SPECIFIC VARIABLES
 ******************************************************************************/

/*******************************************************************************
 * TESTS FUNCTIONS
 ******************************************************************************/

#ifdef DEBUG
	int cnt;
#endif

unsigned int fib(int n)
{
    int i, Fnew, Fold, temp,ans;

    Fnew = 1;  Fold = 0;
    i = 2;
    while( i <= n ) {
      temp = Fnew;
      Fnew = Fnew + Fold;
      Fold = temp;
      i++;
#ifdef DEBUG
	cnt++;
#endif
    }
    ans = Fnew;
#ifdef DEBUG
	OUTPUT("Loop Count : %d\n", cnt);
#endif
  return ans;
}

void* fib_thread(void)
{
    RETURN_CODE_TYPE      ret_type;
    PARTITION_STATUS_TYPE pr_stat;

    uint32_t fibVal;

    GET_PARTITION_STATUS(&pr_stat, &ret_type);
    if(ret_type != NO_ERROR)
    {
        OUTPUT("[FIB] Cannot get partition status [%d]\n", ret_type);
        return (void*)1;
    }

    while(1)
    {
        OUTPUT("Summing fib(%u)\n", FIB_VALUE);
        fibVal = fib(FIB_VALUE);
        OUTPUT("fib(%u) = %u\n", FIB_VALUE, fibVal);

        PERIODIC_WAIT(&ret_type);
        if(ret_type != NO_ERROR)
        {
            OUTPUT("[FIB] Cannot achieve periodic wait [%d]\n", ret_type);
            return (void*)1;
        }
    }

    return (void*)0;
}


/*******************************************************************************
 * TESTS MAIN
 ******************************************************************************/
int main()
{
    PROCESS_ID_TYPE        thread_fib;
    PROCESS_ATTRIBUTE_TYPE th_attr_fib;

    RETURN_CODE_TYPE       ret_type;


    /* Set FIB manipulation process */
    th_attr_fib.ENTRY_POINT   = fib_thread;
    th_attr_fib.DEADLINE      = HARD;
    th_attr_fib.PERIOD        = 100000000;
    th_attr_fib.STACK_SIZE    = 1000;
    th_attr_fib.TIME_CAPACITY = 100000000;
    th_attr_fib.BASE_PRIORITY = 1;
    memcpy(th_attr_fib.NAME, "FIB_A653\0", 9 * sizeof(char));

    OUTPUT("Init P0 partition\n");

    /* Create processes */
    CREATE_PROCESS(&th_attr_fib, &thread_fib, &ret_type);
    if(ret_type != NO_ERROR)
    {
        OUTPUT("Cannot create FIB process [%d]\n", ret_type);
        return -1;
    }

    /* Start all processes */
    START(thread_fib, &ret_type);
    if(ret_type != NO_ERROR)
    {
        OUTPUT("Cannot start FIB process[%d]\n", ret_type);
        return -1;
    }

    /* Parition has been initialized, now switch to normal mode */
    OUTPUT("P0 partition switching to normal mode\n");
    SET_PARTITION_MODE(NORMAL, &ret_type);
    if(ret_type != NO_ERROR)
    {
        OUTPUT("Cannot switch P0 partition to NORMAL state[%d]\n", ret_type);
        return -1;
    }

    STOP_SELF();

    return 0;
}
