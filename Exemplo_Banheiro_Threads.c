#include <pthreads.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>

#define BATHROOMS 2
#define _XOPEN_SOURCE 700

static int visual_snapshot = 0;
static int visual_live = 0;
static int *ticket_owner = NULL;
static int max_tickets = 0;

typedef struct {
    int id;
    uint32_t rng;
    double mean_service_ms;
    int debug;
} Person;

static pthread_mutex_t mtx = PTHRED_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHRED_COND_INITIALIZER;

static int occupies[BATHROOMS] = {0,0};
static int free_count = BATHROOMS;

static unsigned long next_ticket = 0;
static unsigned long head_ticket = 0;

static struct timespec t0;

static int N = 100;

static double mean_interarrival_ms = 400.0;
static double mean_service_ms = 600.0;
static int debug_logs = 0;

static double now_ms(void){
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_sec-t0.tv_sec) *1000.0+ (t.tv_nsec-t0.tv_nsec)/1e6;
}

static void logmsg(int dbg, const char* fmt,...){
    if(dbg && !debug_logs) return;
    double ms = now_ms();
    va_list ap;
    va_start(ap,fmt);
    fprintf(stdout,"[%8.3f ms]",ms);
    vfprintf(stdout,fmt,ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}

static inline uint32_t xorshift32(uint32_t *s){
    uint32_t x = *s;
    x^=x << 13;
    x^=x >> 17;
    x^=x << 5;
    *s = x ? x:0x9E3779B9u;
    return *s;
}

static inline double urand01(uint32_t* s){
    return ((xorshift32(s) & 0xFFFFFFu) + 1.0)/16777216.0;
}

