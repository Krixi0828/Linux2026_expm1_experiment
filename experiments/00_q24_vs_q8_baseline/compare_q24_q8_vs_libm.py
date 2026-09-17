import os
import math
import pandas as pd
import matplotlib.pyplot as plt


CSV_PATH = "expm1_compare.csv"
OUT_DIR = "q24_q8_compare_results"


def rms(values):
    values = list(values)
    if len(values) == 0:
        return 0.0
    return math.sqrt(sum(v * v for v in values) / len(values))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    df = pd.read_csv(CSV_PATH)
    df_nonzero = df[df["x"] > 0].copy()

    # ------------------------------------------------------------
    # 1. 統計 summary
    # ------------------------------------------------------------
    q24_max_abs_idx = df["q24_abs_err"].idxmax()
    q8_max_abs_idx = df["q8_abs_err"].idxmax()

    summary = pd.DataFrame(
        [
            {
                "method": "Q24.8",
                "max_abs_error": df["q24_abs_err"].max(),
                "x_at_max_abs_error": df.loc[q24_max_abs_idx, "x"],
                "mean_abs_error": df["q24_abs_err"].mean(),
                "rms_abs_error": rms(df["q24_abs_err"]),
                "max_relative_error": df_nonzero["q24_rel_err"].max(),
                "mean_relative_error": df_nonzero["q24_rel_err"].mean(),
            },
            {
                "method": "Q8.24",
                "max_abs_error": df["q8_abs_err"].max(),
                "x_at_max_abs_error": df.loc[q8_max_abs_idx, "x"],
                "mean_abs_error": df["q8_abs_err"].mean(),
                "rms_abs_error": rms(df["q8_abs_err"]),
                "max_relative_error": df_nonzero["q8_rel_err"].max(),
                "mean_relative_error": df_nonzero["q8_rel_err"].mean(),
            },
        ]
    )

    summary.to_csv(
        os.path.join(OUT_DIR, "q24_q8_error_summary.csv"),
        index=False,
    )

    print("\n=== Q24.8 vs Q8.24 error summary ===")
    print(summary.to_string(index=False))

    # ------------------------------------------------------------
    # 2. 函數值比較：libm, Q24.8, Q8.24
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["libm_expm1f"], label="libm expm1f(x)")
    plt.plot(df["x"], df["q24_8"], label="Q24.8 Taylor")
    plt.plot(df["x"], df["q8_24"], label="Q8.24 Taylor")
    plt.xlabel("x")
    plt.ylabel("expm1f(x)")
    plt.title("Q24.8 and Q8.24 Taylor approximation vs libm expm1f")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "value_comparison.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 3. absolute error 比較
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["q24_abs_err"], label="Q24.8 absolute error")
    plt.plot(df["x"], df["q8_abs_err"], label="Q8.24 absolute error")
    plt.xlabel("x")
    plt.ylabel("absolute error")
    plt.title("Absolute error comparison: Q24.8 vs Q8.24")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "absolute_error_comparison.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 4. absolute error 比較，log scale
    # ------------------------------------------------------------
    df_log = df[(df["q24_abs_err"] > 0) & (df["q8_abs_err"] > 0)].copy()

    plt.figure(figsize=(10, 6))
    plt.plot(df_log["x"], df_log["q24_abs_err"], label="Q24.8 absolute error")
    plt.plot(df_log["x"], df_log["q8_abs_err"], label="Q8.24 absolute error")
    plt.yscale("log")
    plt.xlabel("x")
    plt.ylabel("absolute error, log scale")
    plt.title("Absolute error comparison, log scale")
    plt.grid(True, which="both")
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "absolute_error_comparison_log.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 5. relative error 比較
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df_nonzero["x"], df_nonzero["q24_rel_err"], label="Q24.8 relative error")
    plt.plot(df_nonzero["x"], df_nonzero["q8_rel_err"], label="Q8.24 relative error")
    plt.xlabel("x")
    plt.ylabel("relative error")
    plt.title("Relative error comparison: Q24.8 vs Q8.24")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "relative_error_comparison.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 6. signed error 比較
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["q24_minus_libm"], label="Q24.8 - libm")
    plt.plot(df["x"], df["q8_minus_libm"], label="Q8.24 - libm")
    plt.axhline(0.0, linestyle="--", linewidth=1)
    plt.xlabel("x")
    plt.ylabel("signed error")
    plt.title("Signed error comparison: Q24.8 vs Q8.24")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "signed_error_comparison.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 7. near-zero 區間比較
    # ------------------------------------------------------------
    df_zoom = df[(df["x"] >= 0.0) & (df["x"] <= 0.05)].copy()

    plt.figure(figsize=(10, 6))
    plt.plot(df_zoom["x"], df_zoom["libm_expm1f"], label="libm expm1f(x)")
    plt.plot(df_zoom["x"], df_zoom["q24_8"], label="Q24.8 Taylor")
    plt.plot(df_zoom["x"], df_zoom["q8_24"], label="Q8.24 Taylor")
    plt.xlabel("x")
    plt.ylabel("expm1f(x)")
    plt.title("Near-zero comparison: Q24.8 vs Q8.24 vs libm")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "near_zero_value_comparison.png"), dpi=200)
    plt.close()

    plt.figure(figsize=(10, 6))
    plt.plot(df_zoom["x"], df_zoom["q24_abs_err"], label="Q24.8 absolute error")
    plt.plot(df_zoom["x"], df_zoom["q8_abs_err"], label="Q8.24 absolute error")
    plt.xlabel("x")
    plt.ylabel("absolute error")
    plt.title("Near-zero absolute error comparison")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "near_zero_absolute_error_comparison.png"), dpi=200)
    plt.close()

    # ------------------------------------------------------------
    # 8. 粗略判斷誰比較好
    # ------------------------------------------------------------
    q24_mean = summary.loc[summary["method"] == "Q24.8", "mean_abs_error"].iloc[0]
    q8_mean = summary.loc[summary["method"] == "Q8.24", "mean_abs_error"].iloc[0]

    print("\n=== 初步判斷 ===")
    if q24_mean < q8_mean:
        print("以 mean absolute error 來看，Q24.8 在這次測試範圍內比較好。")
    elif q8_mean < q24_mean:
        print("以 mean absolute error 來看，Q8.24 在這次測試範圍內比較好。")
    else:
        print("以 mean absolute error 來看，兩者非常接近。")

    print(f"\n圖表與表格已輸出到資料夾：{OUT_DIR}")


if __name__ == "__main__":
    main()