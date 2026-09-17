#include "expm1_fixed.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [--xmin V] [--xmax V] [--step V] [--method NAME]\n"
        "Default: xmin=-10 xmax=4 step=0.001, all methods.\n", prog);
}

int main(int argc, char **argv)
{
    double xmin = -10.0, xmax = 4.0, step = 0.001;
    const char *only_method = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--xmin") == 0 && i + 1 < argc) xmin = atof(argv[++i]);
        else if (strcmp(argv[i], "--xmax") == 0 && i + 1 < argc) xmax = atof(argv[++i]);
        else if (strcmp(argv[i], "--step") == 0 && i + 1 < argc) step = atof(argv[++i]);
        else if (strcmp(argv[i], "--method") == 0 && i + 1 < argc) only_method = argv[++i];
        else if (strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }

    if (!(step > 0.0) || xmax < xmin) {
        fprintf(stderr, "Invalid range.\n");
        return 2;
    }

    size_t method_count;
    const method_desc_t *methods = expm1_methods(&method_count);
    if (only_method && !find_expm1_method(only_method)) {
        fprintf(stderr, "Unknown method: %s\n", only_method);
        return 2;
    }

    printf("method,x,xq,approx,reference,signed_error,abs_error,rel_error,"
           "terms,muls,divs,shifts,adds,saturations,max_abs_intermediate_q,"
           "max_abs_intermediate_real,reduction_steps\n");

    long long points = (long long)floor((xmax - xmin) / step + 0.5) + 1;
    for (long long idx = 0; idx < points; idx++) {
        double x = xmin + (double)idx * step;
        if (x > xmax + step * 1e-9) break;

        int32_t xq = double_to_q8_24(x);
        double ref = expm1(x);

        for (size_t j = 0; j < method_count; j++) {
            if (only_method && strcmp(methods[j].name, only_method) != 0)
                continue;

            qstats_t st;
            qstats_reset(&st);
            int32_t yq = methods[j].fn(xq, &st);
            double approx = q8_24_to_double(yq);
            double signed_err = approx - ref;
            double abs_err = fabs(signed_err);
            double rel_err = (fabs(ref) > 1e-30) ? abs_err / fabs(ref) : NAN;
            double max_intermediate = q8_24_to_double(
                st.max_abs_intermediate_q > INT32_MAX ? INT32_MAX :
                (int32_t)st.max_abs_intermediate_q);
            if (st.max_abs_intermediate_q > INT32_MAX)
                max_intermediate = (double)st.max_abs_intermediate_q / (double)Q8_ONE;

            printf("%s,%.12g,%d,%.12g,%.12g,%.12g,%.12g,",
                   methods[j].name, x, xq, approx, ref, signed_err, abs_err);
            if (isnan(rel_err)) printf("nan,"); else printf("%.12g,", rel_err);
            printf("%d,%llu,%llu,%llu,%llu,%llu,%lld,%.12g,%d\n",
                   st.terms_used,
                   (unsigned long long)st.muls,
                   (unsigned long long)st.divs,
                   (unsigned long long)st.shifts,
                   (unsigned long long)st.adds,
                   (unsigned long long)st.saturations,
                   (long long)st.max_abs_intermediate_q,
                   max_intermediate,
                   st.reduction_steps);
        }
    }
    return 0;
}
