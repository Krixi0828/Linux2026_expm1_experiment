#define _GNU_SOURCE
#include "expm1_fixed.h"

#include <errno.h>
#include <math.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile uint32_t sink_u;

static uint64_t ns_now(void)
{
    struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void try_pin_one_cpu(void)
{
#ifdef __linux__
    cpu_set_t allowed, set;
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0)
        return;
    int chosen = -1;
    for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
        if (CPU_ISSET(cpu, &allowed)) { chosen = cpu; break; }
    }
    if (chosen < 0) return;
    CPU_ZERO(&set);
    CPU_SET(chosen, &set);
    (void)sched_setaffinity(0, sizeof(set), &set);
#endif
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s --method NAME [--samples N] [--repeats N] [--xmin V] [--xmax V] [--scope kernel|e2e]\n"
        "NAME can be libm or any fixed-point method printed by --list.\n", prog);
}

static void list_methods(void)
{
    size_t n;
    const method_desc_t *m = expm1_methods(&n);
    puts("libm");
    for (size_t i = 0; i < n; i++)
        puts(m[i].name);
}

int main(int argc, char **argv)
{
    const char *method_name = NULL;
    const char *scope = "kernel";
    int samples = 2048;
    int repeats = 2000;
    double xmin = -10.0, xmax = 4.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--method") == 0 && i + 1 < argc) method_name = argv[++i];
        else if (strcmp(argv[i], "--samples") == 0 && i + 1 < argc) samples = atoi(argv[++i]);
        else if (strcmp(argv[i], "--repeats") == 0 && i + 1 < argc) repeats = atoi(argv[++i]);
        else if (strcmp(argv[i], "--xmin") == 0 && i + 1 < argc) xmin = atof(argv[++i]);
        else if (strcmp(argv[i], "--xmax") == 0 && i + 1 < argc) xmax = atof(argv[++i]);
        else if (strcmp(argv[i], "--scope") == 0 && i + 1 < argc) scope = argv[++i];
        else if (strcmp(argv[i], "--list") == 0) { list_methods(); return 0; }
        else if (strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }

    if (!method_name || samples <= 0 || repeats <= 0 || xmax <= xmin) {
        usage(argv[0]);
        return 2;
    }
    if (strcmp(scope, "kernel") != 0 && strcmp(scope, "e2e") != 0) {
        fprintf(stderr, "scope must be kernel or e2e\n");
        return 2;
    }

    const method_desc_t *method = NULL;
    int is_libm = strcmp(method_name, "libm") == 0;
    if (!is_libm) {
        method = find_expm1_method(method_name);
        if (!method) {
            fprintf(stderr, "Unknown method: %s\n", method_name);
            return 2;
        }
    }

    int32_t *qinputs = malloc((size_t)samples * sizeof(*qinputs));
    float *finputs = malloc((size_t)samples * sizeof(*finputs));
    if (!qinputs || !finputs) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < samples; i++) {
        double t = (samples == 1) ? 0.0 : (double)i / (double)(samples - 1);
        float x = (float)(xmin + (xmax - xmin) * t);
        finputs[i] = x;
        qinputs[i] = float_to_q8_24_bits(x);
    }

    try_pin_one_cpu();

    /* Warm-up. Use a local XOR accumulator so the anti-optimization sink
       does not dominate the measured function cost. */
    uint32_t warm_acc = 0;
    for (int w = 0; w < 20; w++) {
        for (int i = 0; i < samples; i++) {
            if (is_libm) {
                float y = expm1f(finputs[i]);
                uint32_t u; memcpy(&u, &y, sizeof(u));
                warm_acc ^= u;
            } else if (strcmp(scope, "kernel") == 0) {
                warm_acc ^= (uint32_t)method->fn(qinputs[i], NULL);
            } else {
                int32_t qx = float_to_q8_24_bits(finputs[i]);
                int32_t qy = method->fn(qx, NULL);
                float y = q8_24_to_float_bits(qy);
                uint32_t u; memcpy(&u, &y, sizeof(u));
                warm_acc ^= u;
            }
        }
    }
    sink_u = warm_acc;

    uint32_t acc = 0;
    uint64_t begin = ns_now();
    if (is_libm) {
        for (int r = 0; r < repeats; r++) {
            for (int i = 0; i < samples; i++) {
                float y = expm1f(finputs[i]);
                uint32_t u; memcpy(&u, &y, sizeof(u));
                acc ^= u;
            }
        }
    } else if (strcmp(scope, "kernel") == 0) {
        for (int r = 0; r < repeats; r++)
            for (int i = 0; i < samples; i++)
                acc ^= (uint32_t)method->fn(qinputs[i], NULL);
    } else {
        for (int r = 0; r < repeats; r++) {
            for (int i = 0; i < samples; i++) {
                int32_t qx = float_to_q8_24_bits(finputs[i]);
                int32_t qy = method->fn(qx, NULL);
                float y = q8_24_to_float_bits(qy);
                uint32_t u; memcpy(&u, &y, sizeof(u));
                acc ^= u;
            }
        }
    }
    uint64_t end = ns_now();
    sink_u = acc;

    uint64_t calls = (uint64_t)samples * (uint64_t)repeats;
    uint64_t total_ns = end - begin;
    double ns_per_call = (double)total_ns / (double)calls;

    printf("method,scope,samples,repeats,calls,total_ns,ns_per_call\n");
    printf("%s,%s,%d,%d,%llu,%llu,%.9f\n",
           method_name, scope, samples, repeats,
           (unsigned long long)calls,
           (unsigned long long)total_ns,
           ns_per_call);

    free(qinputs);
    free(finputs);
    return 0;
}
