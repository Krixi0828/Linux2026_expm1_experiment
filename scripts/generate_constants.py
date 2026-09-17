#!/usr/bin/env python3
import math
Q = 1 << 24
for N in (1, 2, 4, 8, 16, 32, 64):
    step = round((math.log(2) / N) * Q)
    table = [round((2 ** (j / N)) * Q) for j in range(N)]
    print(f"N={N}: step_q={step}, pow2_frac_q={table}")
