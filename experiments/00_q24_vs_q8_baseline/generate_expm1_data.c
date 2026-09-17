#include <stdio.h>
#include <math.h>
#include <float.h>

/*
 * 這兩個函式由你自己的 fixed-point 實作提供。
 */
float my_expm1f_q24_8(float x);
float my_expm1f_q8_24(float x);

static void print_one_row(float x)
{
    float ref = expm1f(x);
    float q24 = my_expm1f_q24_8(x);
    float q8  = my_expm1f_q8_24(x);

    float q24_minus_ref = q24 - ref;
    float q8_minus_ref  = q8 - ref;

    float q24_abs_err = fabsf(q24_minus_ref);
    float q8_abs_err  = fabsf(q8_minus_ref);

    float q24_rel_err = 0.0f;
    float q8_rel_err  = 0.0f;

    if (ref != 0.0f) {
        q24_rel_err = q24_abs_err / fabsf(ref);
        q8_rel_err  = q8_abs_err / fabsf(ref);
    }

    float q24_minus_q8 = q24 - q8;

    printf("%.10e,%.10e,%.10e,%.10e,%.10e,%.10e,%.10e,%.10e,%.10e,%.10e,%.10e\n",
           x,
           ref,
           q24,
           q8,
           q24_minus_ref,
           q8_minus_ref,
           q24_abs_err,
           q8_abs_err,
           q24_rel_err,
           q8_rel_err,
           q24_minus_q8);
}

int main(void)
{
    printf("x,libm_expm1f,q24_8,q8_24,q24_minus_libm,q8_minus_libm,q24_abs_err,q8_abs_err,q24_rel_err,q8_rel_err,q24_minus_q8\n");

    /*
     * 先放一些很小的 x。
     * 這些點很重要，因為 Q24.8 的小數解析度只有 1/256。
     */
    float special_points[] = {
        0.0f,
        1.0e-7f,
        1.0e-6f,
        1.0e-5f,
        1.0e-4f,
        5.0e-4f,
        1.0e-3f,
        2.0e-3f,
        3.0e-3f,
        4.0e-3f,
        5.0e-3f,
        1.0e-2f,
        2.0e-2f,
        5.0e-2f,
        1.0e-1f
    };

    int special_count = sizeof(special_points) / sizeof(special_points[0]);

    for (int i = 0; i < special_count; i++) {
        print_one_row(special_points[i]);
    }

    /*
     * 再密集掃描 0 到 1。
     * 你的 Taylor fixed-point 版本目前建議先討論這個範圍。
     */
    for (int i = 0; i <= 10000; i++) {
        float x = (float)i / 10000.0f;
        print_one_row(x);
    }

    return 0;
}