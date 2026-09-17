#include <stdint.h>
#include <string.h>
#include <limits.h>

/*
 * Q24.8 fixed-point format
 *
 * 1.0 = 1 << 8 = 256
 * 0.5 = 128
 *
 * signed int32_t range roughly:
 * [-8388608, 8388608)
 *
 * 小數解析度:
 * 1 / 2^8 = 1 / 256 = 0.00390625
 */
#define Q_FRAC_BITS 8
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
 * 將 positive float 轉成 Q24.8 fixed-point。
 *
 * 不使用 float 乘法，也不使用 float-to-int cast。
 *
 * IEEE 754 single precision:
 *
 *   value = 1.mantissa * 2^(exp - 127)
 *
 * normalized float:
 *
 *   mant = (1 << 23) | frac
 *   value = mant * 2^(exp - 127 - 23)
 *
 * Q24.8:
 *
 *   fixed = value * 2^8
 *         = mant * 2^(exp - 127 - 23 + 8)
 *         = mant * 2^(exp - 142)
 */
static int32_t float_to_q24_8_positive(float x)
{
    uint32_t bits = float_to_bits(x);

    uint32_t sign = bits >> 31;
    uint32_t exp  = (bits >> 23) & 0xFF;
    uint32_t frac = bits & 0x7FFFFF;

    //不討論，所以負數先回傳0
    if (sign != 0) {
        return 0;
    }

    // 0 則直接回傳 0
    if (exp == 0 && frac == 0) {
        return 0;
    }

    // 不處理 subnormal，直街回傳 0
    if (exp == 0) {
        return 0;
    }
    // NaN 先簡化為 INT32_MAX
    if (exp == 0xFF) {
        return INT32_MAX;
    }

    
    //mantissa, 24bits
    uint32_t mant = (1u << 23) | frac;

    /*
     * Q24.8:
     * fixed = mant * 2^(exp - 142)
     */
    int shift = (int)exp - 142;

    if (shift >= 0) {
        /*
         * mant 最多 24 bits。
         * int32_t 正數最多約 31 bits。
         * 所以左移超過 7 bits 可能 overflow。
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

        /*
         * 右移太多代表小到 Q24.8 已經表達不了。
         */
        if (rshift >= 32) {
            return 0;
        }

        /*
         * rounding:
         * 右移前加上 half。
         */
        uint32_t add = 1u << (rshift - 1);
        return (int32_t)((mant + add) >> rshift);
    }
}

/*
 * 將 Q24.8 fixed-point 轉回 positive float。
 *
 * 不使用浮點運算，而是手動組 IEEE 754 float bits。
 *
 * q 的真實值是:
 *
 *   real = q / 2^8
 */
static float q24_8_to_float_positive(int32_t q)
{
    if (q <= 0) {
        return bits_to_float(0);
    }

    uint32_t v = (uint32_t)q;

    /*
     * 找出最高位元的 1。
     */
    int msb = 31;
    while (msb > 0 && ((v & (1u << msb)) == 0)) {
        msb--;
    }

    /*
     * q represents:
     *
     *   real = q / 2^8
     *
     * 若 q 的最高 bit 在 msb，
     * real 的 exponent 為:
     *
     *   msb - 8
     */
    int exp_unbiased = msb - Q_FRAC_BITS;
    uint32_t exp_bits = (uint32_t)(exp_unbiased + 127);

    /*
     * float mantissa 需要 23 bits。
     * 我們要把 v normalize 成：
     *
     *   1.xxxxx * 2^exp
     */
    uint32_t mantissa;

    if (msb > 23) {
        int shift = msb - 23;

        /*
         * 使用 uint64_t 避免 rounding 時 overflow。
         */
        uint64_t rounded = (uint64_t)v + (1ull << (shift - 1));
        uint64_t norm = rounded >> shift;

        /*
         * rounding 後可能變成 10.000...
         * 例如 1.111... rounding 進位。
         */
        if (norm >= (1ull << 24)) {
            norm >>= 1;
            exp_bits++;
        }

        mantissa = (uint32_t)norm & 0x7FFFFF;
    } else {
        int shift = 23 - msb;
        v <<= shift;
        mantissa = v & 0x7FFFFF;
    }

    uint32_t bits = (exp_bits << 23) | mantissa;
    return bits_to_float(bits);
}

/*
 * Q24.8 fixed-point multiplication.
 *
 * 假設：
 *
 *   a_fixed = a * 2^8
 *   b_fixed = b * 2^8
 *
 * 則：
 *
 *   a_fixed * b_fixed = a * b * 2^16
 *
 * 但 Q24.8 的結果應該是：
 *
 *   result = a * b * 2^8
 *
 * 所以要右移 8 bits。
 */
static int32_t q24_8_mul(int32_t a, int32_t b)
{
    int64_t temp = (int64_t)a * (int64_t)b;
    return (int32_t)(temp >> Q_FRAC_BITS);
}

/*
 * my_expm1f_q24_8:
 *
 * 使用 Taylor series:
 *
 *   expm1(x) = x + x^2/2! + x^3/3! + x^4/4! + ...
 *
 * 遞推式：
 *
 *   term_n = term_{n-1} * x / n
 *
 * 限制：
 *
 *   1. 假設 x >= 0
 *   2. 建議先測試 0 <= x <= 1
 *   3. 內部不使用 FPU
 *   4. 使用 Q24.8 fixed-point
 */
float my_expm1f_q24_8(float x)
{
    int32_t xq = float_to_q24_8_positive(x);

    if (xq == 0) {
        return bits_to_float(0);
    }
    int32_t sum = xq;
    int32_t term = xq;
    for (int n = 2; n <= 20; n++) {
        term = q24_8_mul(term, xq);
        term = term / n;

        if (term == 0) {
            break;
        }

        if (INT32_MAX - sum < term) {
            sum = INT32_MAX;
            break;
        }

        sum += term;
    }

    return q24_8_to_float_positive(sum);
}