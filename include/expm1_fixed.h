#ifndef EXPM1_FIXED_H
#define EXPM1_FIXED_H

#include <stdint.h>
#include <stddef.h>

#define Q8_FRAC_BITS 24
#define Q8_ONE ((int32_t)1 << Q8_FRAC_BITS)

typedef enum {
    ARITH_TRUNC = 0,
    ARITH_RNE = 1,
} arith_mode_t;

typedef struct {
    uint64_t muls;
    uint64_t divs;
    uint64_t shifts;
    uint64_t adds;
    uint64_t iterations;
    uint64_t saturations;
    int64_t max_abs_intermediate_q;
    int terms_used;
    int reduction_steps;
} qstats_t;

typedef int32_t (*method_fn_t)(int32_t xq, qstats_t *stats);

typedef struct {
    const char *name;
    const char *description;
    method_fn_t fn;
} method_desc_t;

void qstats_reset(qstats_t *s);

/* Conversion helpers. */
int32_t double_to_q8_24(double x);
double q8_24_to_double(int32_t q);
int32_t float_to_q8_24_bits(float x);
float q8_24_to_float_bits(int32_t q);

/* Method registry. */
const method_desc_t *expm1_methods(size_t *count);
const method_desc_t *find_expm1_method(const char *name);

#endif
