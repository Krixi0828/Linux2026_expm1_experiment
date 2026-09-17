#include "expm1_fixed.h"

#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Q8.24 constants generated offline with round(real * 2^24). */
#define Q_LN2      ((int32_t)11629080)
#define Q_LN2_2    ((int32_t)5814540)
#define Q_LN2_4    ((int32_t)2907270)
#define Q_LN2_8    ((int32_t)1453635)
#define Q_LN2_16   ((int32_t)726817)
#define Q_LN2_32   ((int32_t)363409)
#define Q_LN2_64   ((int32_t)181704)

static const int32_t POW2_FRAC_N1[1] = {
    16777216
};
static const int32_t POW2_FRAC_N2[2] = {
    16777216, 23726566
};
static const int32_t POW2_FRAC_N4[4] = {
    16777216, 19951585, 23726566, 28215802
};
static const int32_t POW2_FRAC_N8[8] = {
    16777216, 18295684, 19951585, 21757357, 23726566, 25874004, 28215802, 30769550
};
static const int32_t POW2_FRAC_N16[16] = {
    16777216, 17520007, 18295684, 19105703, 19951585, 20834917, 21757357, 22720638,
    23726566, 24777031, 25874004, 27019544, 28215802, 29465022, 30769550, 32131834
};
static const int32_t POW2_FRAC_N32[32] = {
    16777216, 17144589, 17520007, 17903645, 18295684, 18696307, 19105703, 19524063,
    19951585, 20388467, 20834917, 21291142, 21757357, 22233781, 22720638, 23218155,
    23726566, 24246111, 24777031, 25319578, 25874004, 26440571, 27019544, 27611195,
    28215802, 28833647, 29465022, 30110222, 30769550, 31443315, 32131834, 32835430
};
static const int32_t POW2_FRAC_N64[64] = {
    16777216, 16959908, 17144589, 17331282, 17520007, 17710787, 17903645, 18098603,
    18295684, 18494911, 18696307, 18899897, 19105703, 19313750, 19524063, 19736666,
    19951585, 20168843, 20388467, 20610483, 20834917, 21061794, 21291142, 21522987,
    21757357, 21994279, 22233781, 22475891, 22720638, 22968049, 23218155, 23470984,
    23726566, 23984932, 24246111, 24510133, 24777031, 25046835, 25319578, 25595290,
    25874004, 26155754, 26440571, 26728490, 27019544, 27313768, 27611195, 27911861,
    28215802, 28523052, 28833647, 29147625, 29465022, 29785875, 30110222, 30438101,
    30769550, 31104608, 31443315, 31785710, 32131834, 32481727, 32835430, 33192984
};

static inline int64_t i64_abs_sat(int64_t x)
{
    if (x >= 0) return x;
    if (x == INT64_MIN) return INT64_MAX;
    return -x;
}

void qstats_reset(qstats_t *s)
{
    if (s) memset(s, 0, sizeof(*s));
}

static inline void track_q(qstats_t *s, int64_t q)
{
    if (!s) return;
    int64_t a = i64_abs_sat(q);
    if (a > s->max_abs_intermediate_q)
        s->max_abs_intermediate_q = a;
}

static inline int32_t sat_i64_to_i32(int64_t x, qstats_t *s)
{
    track_q(s, x);
    if (x > INT32_MAX) {
        if (s) s->saturations++;
        return INT32_MAX;
    }
    if (x < INT32_MIN) {
        if (s) s->saturations++;
        return INT32_MIN;
    }
    return (int32_t)x;
}

static inline int64_t div_rne_i64(int64_t a, int64_t b)
{
    if (b <= 0) return 0;
    if (a >= 0)
        return (a + b / 2) / b;
    return -(((-a) + b / 2) / b);
}

static inline int32_t q_div_small(int32_t a, int n, arith_mode_t mode, qstats_t *s)
{
    if (s) s->divs++;
    int64_t v = (mode == ARITH_RNE) ? div_rne_i64((int64_t)a, n)
                                    : ((int64_t)a / n);
    track_q(s, v);
    return (int32_t)v;
}

static inline int32_t q_add(int32_t a, int32_t b, qstats_t *s)
{
    if (s) s->adds++;
    return sat_i64_to_i32((int64_t)a + (int64_t)b, s);
}

static inline int32_t q_sub(int32_t a, int32_t b, qstats_t *s)
{
    if (s) s->adds++;
    return sat_i64_to_i32((int64_t)a - (int64_t)b, s);
}

static inline int32_t q_mul(int32_t a, int32_t b, arith_mode_t mode, qstats_t *s)
{
    if (s) s->muls++;
    int64_t prod = (int64_t)a * (int64_t)b; /* Q16.48 */
    int64_t scaled;

    if (mode == ARITH_RNE) {
        const int64_t half = (int64_t)1 << (Q8_FRAC_BITS - 1);
        if (prod >= 0)
            scaled = (prod + half) >> Q8_FRAC_BITS;
        else
            scaled = -(((-prod) + half) >> Q8_FRAC_BITS);
    } else {
        /* C-style truncation toward zero after rescaling. */
        if (prod >= 0)
            scaled = prod >> Q8_FRAC_BITS;
        else
            scaled = -(((-prod)) >> Q8_FRAC_BITS);
    }
    return sat_i64_to_i32(scaled, s);
}

static inline int32_t q_shift_right_rne(int32_t a, unsigned sh, qstats_t *s)
{
    if (sh == 0) return a;
    if (s) s->shifts++;
    if (sh >= 31) {
        track_q(s, 0);
        return 0;
    }
    int64_t den = (int64_t)1 << sh;
    int64_t v = div_rne_i64((int64_t)a, den);
    track_q(s, v);
    return (int32_t)v;
}

static inline int32_t q_shift_left_sat(int32_t a, unsigned sh, qstats_t *s)
{
    if (sh == 0) return a;
    if (s) s->shifts++;
    if (sh >= 31)
        return (a >= 0) ? sat_i64_to_i32(INT64_MAX, s) : sat_i64_to_i32(INT64_MIN, s);
    return sat_i64_to_i32((int64_t)a * ((int64_t)1 << sh), s);
}

int32_t double_to_q8_24(double x)
{
    double scaled = x * (double)Q8_ONE;
    if (scaled >= (double)INT32_MAX) return INT32_MAX;
    if (scaled <= (double)INT32_MIN) return INT32_MIN;
    return (int32_t)llround(scaled);
}

double q8_24_to_double(int32_t q)
{
    return (double)q / (double)Q8_ONE;
}

static uint32_t float_to_bits(float x)
{
    uint32_t u;
    memcpy(&u, &x, sizeof(u));
    return u;
}

static float bits_to_float(uint32_t u)
{
    float x;
    memcpy(&x, &u, sizeof(x));
    return x;
}

int32_t float_to_q8_24_bits(float x)
{
    uint32_t bits = float_to_bits(x);
    uint32_t sign = bits >> 31;
    uint32_t exp  = (bits >> 23) & 0xFFu;
    uint32_t frac = bits & 0x7FFFFFu;

    if (exp == 0xFFu)
        return sign ? INT32_MIN : INT32_MAX;
    if (exp == 0u)
        return 0; /* zero/subnormal are below Q8.24 resolution here */

    uint64_t mant = (1u << 23) | frac;
    int shift = (int)exp - 126; /* q = mant * 2^(exp-126) */
    uint64_t mag;

    if (shift >= 0) {
        if (shift > 8) return sign ? INT32_MIN : INT32_MAX;
        mag = mant << shift;
    } else {
        unsigned rshift = (unsigned)(-shift);
        if (rshift >= 64) mag = 0;
        else if (rshift == 0) mag = mant;
        else {
            uint64_t half = (uint64_t)1 << (rshift - 1);
            mag = (mant + half) >> rshift;
        }
    }

    uint64_t limit = sign ? ((uint64_t)INT32_MAX + 1ull) : (uint64_t)INT32_MAX;
    if (mag > limit) return sign ? INT32_MIN : INT32_MAX;
    if (!sign) return (int32_t)mag;
    if (mag == ((uint64_t)INT32_MAX + 1ull)) return INT32_MIN;
    return -(int32_t)mag;
}

float q8_24_to_float_bits(int32_t q)
{
    if (q == 0) return 0.0f;

    uint32_t sign = q < 0 ? 1u : 0u;
    uint32_t mag = (q == INT32_MIN) ? 0x80000000u : (uint32_t)(q < 0 ? -q : q);

    int msb = 31;
    while (msb > 0 && ((mag >> msb) & 1u) == 0u) msb--;

    int exp_unbiased = msb - Q8_FRAC_BITS;
    uint32_t exp_bits = (uint32_t)(exp_unbiased + 127);
    uint64_t norm;

    if (msb > 23) {
        unsigned sh = (unsigned)(msb - 23);
        uint64_t rounded = (uint64_t)mag + ((uint64_t)1 << (sh - 1));
        norm = rounded >> sh;
        if (norm >= (1ull << 24)) {
            norm >>= 1;
            exp_bits++;
        }
    } else {
        norm = (uint64_t)mag << (23 - msb);
    }

    uint32_t mantissa = (uint32_t)norm & 0x7FFFFFu;
    uint32_t bits = (sign << 31) | (exp_bits << 23) | mantissa;
    return bits_to_float(bits);
}

/* ---------- Taylor kernels ---------- */

static int32_t taylor_mul_first(int32_t xq, int max_terms, int adaptive,
                                int32_t eps_q, arith_mode_t mode, qstats_t *s)
{
    if (xq == 0) {
        if (s) s->terms_used = 1;
        return 0;
    }

    int32_t sum = xq;
    int32_t term = xq;
    int used = 1;
    track_q(s, xq);

    for (int n = 2; n <= max_terms; n++) {
        if (s) s->iterations++;
        term = q_mul(term, xq, mode, s);
        term = q_div_small(term, n, mode, s);

        if (term == 0) break;
        sum = q_add(sum, term, s);
        used = n;

        if (adaptive) {
            int64_t a = i64_abs_sat(term);
            if (a <= eps_q) break;
        }
    }

    if (s) s->terms_used = used;
    return sum;
}

static int32_t taylor_div_first(int32_t xq, int max_terms,
                                arith_mode_t mode, qstats_t *s)
{
    if (xq == 0) {
        if (s) s->terms_used = 1;
        return 0;
    }

    int32_t sum = xq;
    int32_t term = xq;
    int used = 1;
    track_q(s, xq);

    for (int n = 2; n <= max_terms; n++) {
        if (s) s->iterations++;
        term = q_div_small(term, n, mode, s);
        term = q_mul(term, xq, mode, s);
        if (term == 0) break;
        sum = q_add(sum, term, s);
        used = n;
    }

    if (s) s->terms_used = used;
    return sum;
}

/* ---------- Range reduction ---------- */

static int64_t round_div_nearest_i64(int64_t a, int64_t b)
{
    return div_rne_i64(a, b);
}

static void floor_divmod_i64(int64_t k, int N, int64_t *a, int *b)
{
    int64_t q = k / N;
    int64_t r = k % N;
    if (r < 0) {
        r += N;
        q -= 1;
    }
    *a = q;
    *b = (int)r;
}

static int32_t reconstruct_pow2(int32_t one_plus_y, int64_t whole_pow,
                                int32_t frac_scale_q, arith_mode_t mode,
                                qstats_t *s)
{
    int32_t scaled = q_mul(one_plus_y, frac_scale_q, mode, s);

    if (whole_pow > 0) {
        if (whole_pow > 30)
            scaled = (scaled >= 0) ? sat_i64_to_i32(INT64_MAX, s)
                                   : sat_i64_to_i32(INT64_MIN, s);
        else
            scaled = q_shift_left_sat(scaled, (unsigned)whole_pow, s);
    } else if (whole_pow < 0) {
        int64_t sh = -whole_pow;
        scaled = q_shift_right_rne(scaled, (unsigned)(sh > 31 ? 31 : sh), s);
    }

    return q_sub(scaled, Q8_ONE, s);
}

static int32_t range_reduce_generic(int32_t xq, int N, int32_t step_q,
                                    const int32_t *frac_table,
                                    int adaptive, int max_terms,
                                    arith_mode_t mode, qstats_t *s)
{
    int64_t k = round_div_nearest_i64((int64_t)xq, (int64_t)step_q);
    int64_t r64 = (int64_t)xq - k * (int64_t)step_q;
    int32_t rq = sat_i64_to_i32(r64, s);

    if (s) s->reduction_steps++;

    int32_t y = taylor_mul_first(rq, max_terms, adaptive, 1, mode, s);
    int32_t one_plus_y = q_add(Q8_ONE, y, s);

    int64_t whole_pow;
    int rem;
    floor_divmod_i64(k, N, &whole_pow, &rem);

    return reconstruct_pow2(one_plus_y, whole_pow, frac_table[rem], mode, s);
}

/* ---------- Repeated-halving reduction ---------- */

static int32_t repeated_halving(int32_t xq, int adaptive, int max_terms,
                                arith_mode_t mode, qstats_t *s)
{
    const int32_t delta_q = Q8_ONE / 2; /* |reduced x| <= 0.5 */
    int32_t rq = xq;
    int m = 0;

    while (i64_abs_sat(rq) > delta_q && m < 30) {
        rq = q_shift_right_rne(rq, 1, s);
        m++;
    }
    if (s) s->reduction_steps += m;

    int32_t y = taylor_mul_first(rq, max_terms, adaptive, 1, mode, s);

    for (int i = 0; i < m; i++) {
        int32_t y2 = q_mul(y, y, mode, s);
        int32_t two_y = q_shift_left_sat(y, 1, s);
        y = q_add(two_y, y2, s); /* expm1(2a) = 2y + y^2 */
    }
    return y;
}

/* ---------- Public method wrappers ---------- */

static int32_t m_direct_t12_trunc(int32_t xq, qstats_t *s)
{ return taylor_mul_first(xq, 12, 0, 0, ARITH_TRUNC, s); }

static int32_t m_direct_t12_rne(int32_t xq, qstats_t *s)
{ return taylor_mul_first(xq, 12, 0, 0, ARITH_RNE, s); }

static int32_t m_direct_t16_rne(int32_t xq, qstats_t *s)
{ return taylor_mul_first(xq, 16, 0, 0, ARITH_RNE, s); }

static int32_t m_direct_t20_rne(int32_t xq, qstats_t *s)
{ return taylor_mul_first(xq, 20, 0, 0, ARITH_RNE, s); }

static int32_t m_adaptive_rne(int32_t xq, qstats_t *s)
{ return taylor_mul_first(xq, 48, 1, 1, ARITH_RNE, s); }

static int32_t m_divfirst_t12_rne(int32_t xq, qstats_t *s)
{ return taylor_div_first(xq, 12, ARITH_RNE, s); }

static int32_t m_rr_ln2_t12(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 1, Q_LN2, POW2_FRAC_N1, 0, 12, ARITH_RNE, s); }

static int32_t m_rr_ln2_adaptive(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 1, Q_LN2, POW2_FRAC_N1, 1, 48, ARITH_RNE, s); }

static int32_t m_halving_t12(int32_t xq, qstats_t *s)
{ return repeated_halving(xq, 0, 12, ARITH_RNE, s); }

static int32_t m_halving_adaptive(int32_t xq, qstats_t *s)
{ return repeated_halving(xq, 1, 48, ARITH_RNE, s); }

static int32_t m_fine_ln2_2_t12(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 2, Q_LN2_2, POW2_FRAC_N2, 0, 12, ARITH_RNE, s); }

static int32_t m_fine_ln2_4_t12(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 4, Q_LN2_4, POW2_FRAC_N4, 0, 12, ARITH_RNE, s); }

static int32_t m_fine_ln2_8_t12(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 8, Q_LN2_8, POW2_FRAC_N8, 0, 12, ARITH_RNE, s); }

static int32_t m_fine_ln2_16_t12(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 16, Q_LN2_16, POW2_FRAC_N16, 0, 12, ARITH_RNE, s); }

static int32_t m_fine_ln2_32_t12(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 32, Q_LN2_32, POW2_FRAC_N32, 0, 12, ARITH_RNE, s); }

static int32_t m_fine_ln2_64_t12(int32_t xq, qstats_t *s)
{ return range_reduce_generic(xq, 64, Q_LN2_64, POW2_FRAC_N64, 0, 12, ARITH_RNE, s); }

static const method_desc_t METHODS[] = {
    {"direct_t12_trunc", "Direct Taylor-12, truncating fixed-point arithmetic", m_direct_t12_trunc},
    {"direct_t12_rne", "Direct Taylor-12, round-to-nearest arithmetic", m_direct_t12_rne},
    {"direct_t16_rne", "Direct Taylor-16", m_direct_t16_rne},
    {"direct_t20_rne", "Direct Taylor-20", m_direct_t20_rne},
    {"adaptive_rne", "Adaptive Taylor, stop at <= 1 Q8.24 LSB or 48 terms", m_adaptive_rne},
    {"divfirst_t12_rne", "Taylor-12 with (term/n)*x ordering", m_divfirst_t12_rne},
    {"rr_ln2_t12", "ln2 range reduction + Taylor-12", m_rr_ln2_t12},
    {"rr_ln2_adaptive", "ln2 range reduction + adaptive Taylor", m_rr_ln2_adaptive},
    {"halving_t12", "Repeated halving + Taylor-12 + y<-2y+y^2 reconstruction", m_halving_t12},
    {"halving_adaptive", "Repeated halving + adaptive Taylor", m_halving_adaptive},
    {"fine_ln2_2_t12", "ln2/2 fine-grain reduction + Taylor-12", m_fine_ln2_2_t12},
    {"fine_ln2_4_t12", "ln2/4 fine-grain reduction + Taylor-12", m_fine_ln2_4_t12},
    {"fine_ln2_8_t12", "ln2/8 fine-grain reduction + Taylor-12", m_fine_ln2_8_t12},
    {"fine_ln2_16_t12", "ln2/16 fine-grain reduction + Taylor-12", m_fine_ln2_16_t12},
    {"fine_ln2_32_t12", "ln2/32 fine-grain reduction + Taylor-12", m_fine_ln2_32_t12},
    {"fine_ln2_64_t12", "ln2/64 fine-grain reduction + Taylor-12", m_fine_ln2_64_t12},
};

const method_desc_t *expm1_methods(size_t *count)
{
    if (count) *count = sizeof(METHODS) / sizeof(METHODS[0]);
    return METHODS;
}

const method_desc_t *find_expm1_method(const char *name)
{
    size_t n;
    const method_desc_t *m = expm1_methods(&n);
    for (size_t i = 0; i < n; i++)
        if (strcmp(m[i].name, name) == 0)
            return &m[i];
    return NULL;
}
