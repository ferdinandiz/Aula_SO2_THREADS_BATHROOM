#include <pthread.h>
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

static int visual_snapshots = 0;
static int visual_live = 0;
static int *ticket_owner = NULL;
static int max_tickets = 0;

typedef struct {
    int id;
    uint32_t rng;
    double mean_service_ms;
    int debug;
} Person;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv  = PTHREAD_COND_INITIALIZER;

static int occupied[BATHROOMS] = {0,0};
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

static useconds_t expo_sleep_us(uint32_t* rng_state, double mean_ms) {
    if (mean_ms <= 0.0) return 0;
    double U = urand01(rng_state);
    double x_ms = -mean_ms * log(U);
    if (x_ms < 0.0) x_ms = 0.0;
    return (useconds_t)(x_ms * 1000.0);
}


static void clear_screen_portable(void) {
    if (visual_live) {
        // tenta ANSI
        fprintf(stdout, "\033[2J\033[H");
        fflush(stdout);
        // ou
        system("cls");
    }
}

static void print_header(void) {
    if (visual_live) {
        fprintf(stdout, "--- Painel SO2: Banheiros x Fila (live) ---\n");
    }
}

static void print_state_locked(const char* reason) {
    if (!visual_snapshots && !visual_live) return;

    if (visual_live) {
        clear_screen_portable();
        print_header();
    } else {
        fprintf(stdout, "\n--- %s ---\n", reason);
    }

    char buf[128];
    int n = 0;
    for (int i = 0; i < BATHROOMS; ++i) {
        if (occupied[i] == 0) n += snprintf(buf + n, sizeof(buf) - n, "|  | ");
        else                  n += snprintf(buf + n, sizeof(buf) - n, "|%02d| ", occupied[i]);
    }

    fprintf(stdout, "Banheiros: %s (livres=%d)\n", buf, free_count);


    fprintf(stdout, "Fila: [");
    for (unsigned long t = head_ticket; t < next_ticket; ++t) {
        int id = (t < (unsigned long)max_tickets) ? ticket_owner[t] : -1;
        if (id > 0) fprintf(stdout, " %d", id);
        else        fprintf(stdout, " ?");
    }
    fprintf(stdout, " ]  (tickets head=%lu, next=%lu)\n", head_ticket, next_ticket);

    if (!visual_live) {

        fprintf(stdout, "(evento: %s)\n", reason);
    }
    fflush(stdout);
}


static int occupy_bathroom_locked(int person_id) {
    for (int i = 0; i < BATHROOMS; ++i) {
        if (occupied[i] == 0) {
            occupied[i] = person_id;
            free_count--;
            return i;
        }
    }
    return -1;
}

static void release_bathroom_locked(int i) {
    occupied[i] = 0;
    free_count++;
}


static void* person_thread(void* arg) {
    Person* p = (Person*)arg;

    pthread_mutex_lock(&mtx);
    unsigned long my_ticket = next_ticket++;
    if (my_ticket < (unsigned long)max_tickets) ticket_owner[my_ticket] = p->id;
    logmsg(1, "Pessoa %d chegou (ticket %lu). Fila: head=%lu, livres=%d",
           p->id, my_ticket, head_ticket, free_count);
    print_state_locked("chegada");

    int my_bathroom = -1;
    // Espera sua vez E vaga
    while (!(my_ticket == head_ticket && free_count > 0)) {
        pthread_cond_wait(&cv, &mtx);

        if (visual_live) print_state_locked("aguardando");
    }
    // Entra
    my_bathroom = occupy_bathroom_locked(p->id);
    head_ticket++;

    int b = my_bathroom;
    logmsg(0, "Pessoa %d ENTROU no banheiro %d (tickets: head=%lu, next=%lu, livres=%d)",
           p->id, b, head_ticket, next_ticket, free_count);
    print_state_locked("entrou");
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);

    // Usa o banheiro
    useconds_t svc = expo_sleep_us(&p->rng, p->mean_service_ms);
    usleep(svc);

    // Sai
    pthread_mutex_lock(&mtx);
    release_bathroom_locked(b);
    logmsg(0, "Pessoa %d SAIU do banheiro %d (livres=%d)", p->id, b, free_count);
    print_state_locked("saiu");
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);

    return NULL;
}

// ===== CLI =====
static void usage(const char* prog) {
    fprintf(stderr,
        "Uso: %s [-n pessoas] [-i mean_interarrival_ms] [-t mean_service_ms] [-s seed] [-d] [-v|-V]\n"
        "   -v  snapshots a cada evento (chegada/entrada/saida)\n"
        "   -V  painel ao vivo (limpa e reescreve)\n"
        "Ex.: %s -n 30 -i 120 -t 500 -s 123 -d -V\n", prog, prog);
}

int main(int argc, char** argv) {
    int opt; unsigned int seed = (unsigned int)time(NULL);
    while ((opt = getopt(argc, argv, "n:i:t:s:dvV")) != -1) {
        switch (opt) {
            case 'n': N = atoi(optarg); break;
            case 'i': mean_interarrival_ms = atof(optarg); break;
            case 't': mean_service_ms     = atof(optarg); break;
            case 's': seed = (unsigned int)strtoul(optarg, NULL, 10); break;
            case 'd': debug_logs = 1; break;
            case 'v': visual_snapshots = 1; break;
            case 'V': visual_live = 1; break;
            default: usage(argv[0]); return 1;
        }
    }
    if (visual_snapshots && visual_live) {
        fprintf(stderr, "Use apenas -v OU -V (não ambos).\n");
        return 1;
    }

    printf("=== Simulacao: %d pessoas, %d banheiros, ia=%.1f ms, st=%.1f ms, seed=%u ===\n",
           N, BATHROOMS, mean_interarrival_ms, mean_service_ms, seed);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &t0);

    pthread_t* th = (pthread_t*)malloc(sizeof(pthread_t) * N);
    Person*    ps = (Person*)   malloc(sizeof(Person) * N);
    if (!th || !ps) { perror("malloc"); return 1; }

    max_tickets = N;
    ticket_owner = (int*)calloc((size_t)max_tickets, sizeof(int));
    if (!ticket_owner) { perror("calloc ticket_owner"); return 1; }

    // estado RNG global p/ gerar seeds por thread
    uint32_t grng = seed ? seed : 1u;

    // Mostra estado inicial (vazio) para quem ligou visual
    pthread_mutex_lock(&mtx);
    print_state_locked("inicial");
    pthread_mutex_unlock(&mtx);

    // cria N pessoas com chegadas ~exponenciais
    for (int i = 0; i < N; ++i) {
        ps[i].id = i + 1;
        ps[i].rng = xorshift32(&grng);
        ps[i].mean_service_ms = mean_service_ms;
        ps[i].debug = debug_logs;

        // atraso de chegada antes de criar a próxima
        useconds_t arr = expo_sleep_us(&grng, mean_interarrival_ms);
        usleep(arr);

        int rc = pthread_create(&th[i], NULL, person_thread, &ps[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create falhou (%d): %s\n", rc, strerror(rc));
            return 2;
        }
    }

    for (int i = 0; i < N; ++i) {
        pthread_join(th[i], NULL);
    }

    free(th); free(ps); free(ticket_owner);
    printf("=== Fim: todos atendidos e sairam. ===\n");
    return 0;
}
