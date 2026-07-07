#include <stdint.h>
#include <string.h>

/*
 * Q8.24 fixed-point format
 *
 * 1.0  = 1 << 24
 * 0.5  = 0.5 * 2^24
 *
 * signed int32_t range roughly:
 * [-128, 128)
 */
#define Q_FRAC_BITS 24
#define Q_ONE       (1 << Q_FRAC_BITS)

/*
 * 重新解釋 float bits，不做浮點運算。
 */
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

/*
 * 將 positive float 轉成 Q8.24 fixed-point。
 *
 * 注意：
 * 這裡不使用 float 乘法。
 * 直接拆 IEEE 754 float:
 *
 * float value = 1.mantissa * 2^(exp - 127)
 *
 * 對於 normalized positive float:
 * fixed = value * 2^24
 */
static int32_t float_to_q8_24_positive(float x)
{
    uint32_t bits = float_to_bits(x);

    uint32_t sign = bits >> 31;
    uint32_t exp  = (bits >> 23) & 0xFF;
    uint32_t frac = bits & 0x7FFFFF;

    if (sign != 0) {
        return 0;
    }

    if (exp == 0 && frac == 0) {
        return 0;
    }

    /*
     * 這版先不處理 subnormal。
     * 對非常非常小的 x，直接視為 0。
     */
    if (exp == 0) {
        return 0;
    }

    /*
     * mantissa 是 24-bit:
     * 1.frac = (1 << 23) | frac
     */
    uint32_t mant = (1u << 23) | frac;

    /*
     * value = mant * 2^(exp - 127 - 23)
     * fixed = value * 2^24
     *       = mant * 2^(exp - 127 - 23 + 24)
     *       = mant * 2^(exp - 126)
     */
    int shift = (int)exp - 126;

    if (shift >= 0) {
        /*
         * 如果 shift 太大，Q8.24 會 overflow。
         * 這裡簡化處理：飽和到最大正數。
         */
        if (shift > 7) {
            return INT32_MAX;
        }
        uint64_t v = (uint64_t)mant << shift;
        if (v > (uint64_t)INT32_MAX) {
            return INT32_MAX;
        }
        return (int32_t)v;
    } else {
        int rshift = -shift;
        if (rshift >= 32) {
            return 0;
        }

        /*
         * 加入 rounding：右移前加 half。
         */
        uint32_t add = 1u << (rshift - 1);
        return (int32_t)((mant + add) >> rshift);
    }
}

/*
 * 將 Q8.24 fixed-point 轉回 float。
 *
 * 這裡一樣不用浮點運算，而是手動組 IEEE 754 float bits。
 * 這版只處理非負數。
 */
static float q8_24_to_float_positive(int32_t q)
{
    if (q <= 0) {
        return bits_to_float(0);
    }

    uint32_t v = (uint32_t)q;

    /*
     * 找最高的 1 在第幾個 bit。
     */
    int msb = 31;
    while (msb > 0 && ((v & (1u << msb)) == 0)) {
        msb--;
    }

    /*
     * q represents:
     * real_value = q / 2^24
     *
     * 若 q 的最高位在 msb，
     * real_value 的 exponent 是 msb - 24。
     */
    int exp_unbiased = msb - Q_FRAC_BITS;
    uint32_t exp_bits = (uint32_t)(exp_unbiased + 127);

    /*
     * float mantissa 需要 23 bits。
     * 目標是把 v normalize 成：
     * 1.xxxxx * 2^exp
     */
    uint32_t mantissa;

    if (msb > 23) {
        int shift = msb - 23;

        /*
         * rounding
         */
        uint32_t half = 1u << (shift - 1);
        uint32_t rounded = v + half;

        /*
         * rounding 可能造成進位，使 msb 改變。
         */
        if (rounded < v) {
            rounded = v;
        }

        v = rounded >> shift;

        if (v >= (1u << 24)) {
            v >>= 1;
            exp_bits++;
        }

        mantissa = v & 0x7FFFFF;
    } else {
        int shift = 23 - msb;
        v <<= shift;
        mantissa = v & 0x7FFFFF;
    }

    uint32_t bits = (exp_bits << 23) | mantissa;
    return bits_to_float(bits);
}

/*
 * Q8.24 fixed-point multiplication:
 *
 * result = (a * b) >> 24
 */
static int32_t q_mul(int32_t a, int32_t b)
{
    int64_t temp = (int64_t)a * (int64_t)b;
    return (int32_t)(temp >> Q_FRAC_BITS);
}

/*
 * my_expm1f:
 *
 * 計算 exp(x) - 1
 * 使用：
 * expm1(x) = x + x^2/2! + x^3/3! + ...
 *
 * 限制：
 * 1. 假設 x >= 0
 * 2. 建議先測 0 <= x <= 1
 * 3. 內部不使用 FPU
 */
float my_expm1f_q8_24(float x)
{
    int32_t xq = float_to_q8_24_positive(x);

    if (xq == 0) {
        return bits_to_float(0);
    }

    /*
     * sum = x
     * term = x
     */
    int32_t sum = xq;
    int32_t term = xq;

    /*
     * Taylor series:
     *
     * term_n = term_{n-1} * x / n
     *
     * 這裡取到 n = 12。
     * 對 0 <= x <= 1 通常已經夠用。
     */
    for (int n = 2; n <= 12; n++) {
        term = q_mul(term, xq);
        term = term / n;

        if (term == 0) {
            break;
        }

        /*
         * 簡單 overflow 保護。
         */
        if (INT32_MAX - sum < term) {
            sum = INT32_MAX;
            break;
        }

        sum += term;
    }

    return q8_24_to_float_positive(sum);
}