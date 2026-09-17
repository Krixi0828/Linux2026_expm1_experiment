import os
import pandas as pd
import matplotlib.pyplot as plt


CSV_PATH = "expm1_compare.csv"
OUT_DIR = "q24_vs_libm_results"


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    df = pd.read_csv(CSV_PATH)

    # 避免 x = 0 時 relative error 不好解釋
    df_nonzero = df[df["x"] > 0].copy()

    # ------------------------------------------------------------
    # 1. 輸出 Q24.8 和 libm expm1f 的差值表
    # ------------------------------------------------------------
    table_points = [
        0.0,
        1e-7,
        1e-6,
        1e-5,
        1e-4,
        5e-4,
        1e-3,
        2e-3,
        3e-3,
        4e-3,
        5e-3,
        1e-2,
        2e-2,
        5e-2,
        1e-1,
        2e-1,
        5e-1,
        1.0,
    ]

    rows = []
    for target_x in table_points:
        idx = (df["x"] - target_x).abs().idxmin()
        rows.append(df.loc[idx])

    table = pd.DataFrame(rows)

    table = table[
        [
            "x",
            "libm_expm1f",
            "q24_8",
            "q24_minus_libm",
            "q24_abs_err",
            "q24_rel_err",
        ]
    ]

    table.to_csv(
        os.path.join(OUT_DIR, "q24_vs_libm_diff_table.csv"),
        index=False,
    )

    print("\n=== Q24.8 vs libm expm1f 差值表 ===")
    print(table.to_string(index=False))

    # ------------------------------------------------------------
    # 2. 函數值比較圖：Q24.8 vs libm expm1f
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["libm_expm1f"], label="libm expm1f(x)")
    plt.plot(df["x"], df["q24_8"], label="my_expm1f Q24.8")
    plt.xlabel("x")
    plt.ylabel("expm1f(x)")
    plt.title("Q24.8 Taylor approximation vs libm expm1f")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "q24_vs_libm_value.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 3. signed error：q24 - libm
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["q24_minus_libm"], label="Q24.8 - libm")
    plt.axhline(0.0, linestyle="--", linewidth=1)
    plt.xlabel("x")
    plt.ylabel("signed error")
    plt.title("Signed error of Q24.8 Taylor approximation")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "q24_signed_error.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 4. absolute error
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["q24_abs_err"], label="absolute error")
    plt.xlabel("x")
    plt.ylabel("absolute error")
    plt.title("Absolute error of Q24.8 Taylor approximation")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "q24_absolute_error.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 5. absolute error, log scale
    # ------------------------------------------------------------
    df_abs_positive = df[df["q24_abs_err"] > 0].copy()

    plt.figure(figsize=(10, 6))
    plt.plot(df_abs_positive["x"], df_abs_positive["q24_abs_err"], label="absolute error")
    plt.yscale("log")
    plt.xlabel("x")
    plt.ylabel("absolute error, log scale")
    plt.title("Absolute error of Q24.8 Taylor approximation, log scale")
    plt.grid(True, which="both")
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "q24_absolute_error_log.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 6. relative error
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df_nonzero["x"], df_nonzero["q24_rel_err"], label="relative error")
    plt.xlabel("x")
    plt.ylabel("relative error")
    plt.title("Relative error of Q24.8 Taylor approximation")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "q24_relative_error.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 7. zoom in near zero
    # ------------------------------------------------------------
    df_zoom = df[(df["x"] >= 0.0) & (df["x"] <= 0.05)].copy()

    plt.figure(figsize=(10, 6))
    plt.plot(df_zoom["x"], df_zoom["libm_expm1f"], label="libm expm1f(x)")
    plt.plot(df_zoom["x"], df_zoom["q24_8"], label="my_expm1f Q24.8")
    plt.xlabel("x")
    plt.ylabel("expm1f(x)")
    plt.title("Q24.8 vs libm near zero")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "q24_vs_libm_near_zero.png"), dpi=200)
    plt.close()

    print(f"\n圖表與表格已輸出到資料夾：{OUT_DIR}")


if __name__ == "__main__":
    main()