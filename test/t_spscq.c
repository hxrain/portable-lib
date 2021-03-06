/*
 * SPSC Queue Tests
 * Producer-consumer tests with two threads.
 *
 * (c) 2015 Sudhi Herle <sw-at-herle.net> 
 *
 * Licensing Terms: GPLv2 
 *
 * If you need a commercial license for this work, please contact
 * the author.
 *
 * This software does not come with any express or implied
 * warranty; it is provided "as is". No claim  is made to its
 * suitability for any purpose.
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include "utils/utils.h"
#include "fast/spsc_bounded_queue.h"
#include "utils/cpu.h"
#include "error.h"


#define QSIZ        8192

// Simple Queue of ints
SPSCQ_TYPEDEF(pcq, int, QSIZ);

struct ctx
{
    pcq q;

    atomic_uint_fast32_t done;

    // Producer cycles
    uint64_t pcyc;  // time taken by producer
    uint64_t ploop; // number of producer iterations

    uint64_t ccyc;  // time taken by consumer
    uint64_t cloop; // number of consumer iterations
};
typedef struct ctx ctx;

static void
mt_setup(ctx* c)
{
    SPSCQ_INIT(&c->q, QSIZ);
    atomic_init(&c->done, 0);

    c->pcyc = c->ccyc = c->ploop = c->cloop = 0;
}




static void*
producer(void* v)
{
    ctx* c = v;
    pcq* q = &c->q;

    /* Push into queue until it fills */
    do {
        uint64_t t0 = sys_cpu_timestamp();

        if (!SPSCQ_ENQ(q, c->ploop)) break;

        c->pcyc += (sys_cpu_timestamp() - t0);
        c->ploop++;
    } while (1);

    atomic_store(&c->done, 1);

    return 0;
}



// Drain the queue of all elements and verify that what we removed
// is what we expect
static inline int
drain(ctx* c, int i, int done)
{
    pcq*q = &c->q;
    int j = 0;
    int n = 0;

    do {
        uint64_t t0 = sys_cpu_timestamp();
        if (!SPSCQ_DEQ(q, j)) break;

        c->ccyc += (sys_cpu_timestamp() - t0);
        c->cloop++;

        if (j != i)
            error(1, 0, "deq mismatch; exp %d, saw %d [n %d%s]\n",
                    i, j, n, done ? " DONE" : "");
        ++i;
    } while (1);
    return i;
}


static void*
consumer(void* v)
{
    ctx* c = v;
    int  i = 0;

    while (!atomic_load(&c->done)) {
        i  = drain(c, i, 0);
    }

    // Go through one last time - the producer may have put
    // something in there between the time we drained and checked
    // the atomic_load().
    i = drain(c, i, 1);

    return 0;
}


static void
mt_test()
{
    ctx cx;
    int ncpu = sys_cpu_getavail();
    int cpu  = 0;

    mt_setup(&cx);

    pthread_t p, c; // producer & consumer
    int r;

    if ((r = pthread_create(&p, 0, producer, &cx)) != 0)
        error(1, r, "Can't create producer thread");

    if (cpu < ncpu)
        sys_cpu_set_thread_affinity(p, cpu++);

    if ((r = pthread_create(&c, 0, consumer, &cx)) != 0)
        error(1, r, "Can't create consumer thread");

    if (cpu < ncpu)
        sys_cpu_set_thread_affinity(c, cpu++);


    pthread_join(p, 0);
    pthread_join(c, 0);

    if (cx.ploop != cx.cloop)
        error(1, 0, "IN/OUT mismatch. in %d, out %d", cx.ploop, cx.cloop);


#define dd(x)   ((double)(x))
    uint64_t n  = cx.ploop;

    printf("%" PRIu64 " items; %5.2f cyc/producer %5.2f cyc/consumer\n", n,
            dd(cx.pcyc)/dd(n), dd(cx.ccyc) / dd(n));

    memset(&cx, 0x5a, sizeof cx);
}


static void
basic_test()
{
    /* Declare a local queue of 4 slots */
    SPSCQ_TYPEDEF(qq_type, int, 4);

    qq_type zq;
    qq_type* q = &zq;
    int s;

    SPSCQ_INIT(q, 4);

    s = SPSCQ_ENQ(q, 10);      assert(s == 1);
    s = SPSCQ_ENQ(q, 11);      assert(s == 1);
    s = SPSCQ_ENQ(q, 12);      assert(s == 1);

    s = SPSCQ_ENQ(q, 13);      assert(s == 0);


    int j;
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 10);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 11);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 12);

    s = SPSCQ_DEQ(q, j);       assert(s == 0);

    s = SPSCQ_ENQ(q, 20);      assert(s == 1);
    s = SPSCQ_ENQ(q, 21);      assert(s == 1);
    s = SPSCQ_ENQ(q, 22);      assert(s == 1);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 20);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 21);

    s = SPSCQ_ENQ(q, 23);      assert(s == 1);
    s = SPSCQ_ENQ(q, 24);      assert(s == 1);

    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 22);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 23);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 24);
    s = SPSCQ_DEQ(q, j);       assert(s == 0);
}

static void
basic_dyn_test()
{
    SPSCQ_DYN_TYPEDEF(dq_type, int);

    dq_type zq;
    dq_type* q = &zq;
    int s;

    SPSCQ_DYN_INIT(q, 4);

    s = SPSCQ_ENQ(q, 10);      assert(s == 1);
    s = SPSCQ_ENQ(q, 11);      assert(s == 1);
    s = SPSCQ_ENQ(q, 12);      assert(s == 1);

    s = SPSCQ_ENQ(q, 13);      assert(s == 0);


    int j;
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 10);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 11);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 12);

    s = SPSCQ_DEQ(q, j);       assert(s == 0);

    s = SPSCQ_ENQ(q, 20);      assert(s == 1);
    s = SPSCQ_ENQ(q, 21);      assert(s == 1);
    s = SPSCQ_ENQ(q, 22);      assert(s == 1);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 20);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 21);

    s = SPSCQ_ENQ(q, 23);      assert(s == 1);
    s = SPSCQ_ENQ(q, 24);      assert(s == 1);

    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 22);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 23);
    s = SPSCQ_DEQ(q, j);       assert(s == 1); assert(j == 24);
    s = SPSCQ_DEQ(q, j);       assert(s == 0);

    SPSCQ_DYN_FINI(q);
}

int
main(int argc, char *argv[])
{
    (void)argc;
    program_name = argv[0];

    basic_test();
    basic_dyn_test();

    int i = 0;
    int n = 32;
    for (i = 0; i < n; ++i) {
        mt_test();
        usleep(500 * 1000);
    }

    return 0;
}
