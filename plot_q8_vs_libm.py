import os
import pandas as pd

os.environ.setdefault("MPLCONFIGDIR", os.path.join(os.getcwd(), ".mplconfig"))

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


CSV_PATH = "expm1_compare.csv"
OUT_DIR = "q8_vs_libm_results"


REQUIRED_COLUMNS = [
    "x",
    "libm_expm1f",
    "q8_24",
    "q8_minus_libm",
    "q8_abs_err",
    "q8_rel_err",
]


def save_plot(path):
    plt.tight_layout()
    plt.savefig(path, dpi=200)
    plt.close()


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    df = pd.read_csv(CSV_PATH)
    missing = [col for col in REQUIRED_COLUMNS if col not in df.columns]
    if missing:
        raise ValueError(f"Missing columns in {CSV_PATH}: {', '.join(missing)}")

    # Avoid x = 0 because relative error is not meaningful there.
    df_nonzero = df[df["x"] > 0].copy()

    # ------------------------------------------------------------
    # 1. Output Q8.24 and libm expm1f difference table
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
            "q8_24",
            "q8_minus_libm",
            "q8_abs_err",
            "q8_rel_err",
        ]
    ]

    table.to_csv(
        os.path.join(OUT_DIR, "q8_vs_libm_diff_table.csv"),
        index=False,
    )

    print("\n=== Q8.24 vs libm expm1f difference table ===")
    print(table.to_string(index=False))

    # ------------------------------------------------------------
    # 2. Value comparison: Q8.24 vs libm expm1f
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["libm_expm1f"], label="libm expm1f(x)")
    plt.plot(df["x"], df["q8_24"], label="my_expm1f Q8.24")
    plt.xlabel("x")
    plt.ylabel("expm1f(x)")
    plt.title("Q8.24 Taylor approximation vs libm expm1f")
    plt.grid(True)
    plt.legend()
    save_plot(os.path.join(OUT_DIR, "q8_vs_libm_value.png"))

    # ------------------------------------------------------------
    # 3. Signed error: q8 - libm
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["q8_minus_libm"], label="Q8.24 - libm")
    plt.axhline(0.0, linestyle="--", linewidth=1)
    plt.xlabel("x")
    plt.ylabel("signed error: Q8.24 - libm")
    plt.title("Signed error of Q8.24 Taylor approximation")
    plt.grid(True)
    plt.legend()
    save_plot(os.path.join(OUT_DIR, "q8_signed_error.png"))

    # ------------------------------------------------------------
    # 4. Absolute error
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df["x"], df["q8_abs_err"], label="Q8.24 absolute error")
    plt.xlabel("x")
    plt.ylabel("absolute error: |Q8.24 - libm|")
    plt.title("Absolute error of Q8.24 Taylor approximation")
    plt.grid(True)
    plt.legend()
    save_plot(os.path.join(OUT_DIR, "q8_absolute_error.png"))

    # ------------------------------------------------------------
    # 5. Absolute error, log scale
    # ------------------------------------------------------------
    df_abs_positive = df[df["q8_abs_err"] > 0].copy()

    plt.figure(figsize=(10, 6))
    plt.plot(
        df_abs_positive["x"],
        df_abs_positive["q8_abs_err"],
        label="Q8.24 absolute error",
    )
    plt.yscale("log")
    plt.xlabel("x")
    plt.ylabel("absolute error: |Q8.24 - libm|, log scale")
    plt.title("Absolute error of Q8.24 Taylor approximation, log scale")
    plt.grid(True, which="both")
    plt.legend()
    save_plot(os.path.join(OUT_DIR, "q8_absolute_error_log.png"))

    # ------------------------------------------------------------
    # 6. Relative error
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df_nonzero["x"], df_nonzero["q8_rel_err"], label="Q8.24 relative error")
    plt.xlabel("x")
    plt.ylabel("relative error: |Q8.24 - libm| / |libm|")
    plt.title("Relative error of Q8.24 Taylor approximation")
    plt.grid(True)
    plt.legend()
    save_plot(os.path.join(OUT_DIR, "q8_relative_error.png"))

    # ------------------------------------------------------------
    # 7. Zoom in near zero: value comparison
    # ------------------------------------------------------------
    df_zoom = df[(df["x"] >= 0.0) & (df["x"] <= 0.05)].copy()

    plt.figure(figsize=(10, 6))
    plt.plot(df_zoom["x"], df_zoom["libm_expm1f"], label="libm expm1f(x)")
    plt.plot(df_zoom["x"], df_zoom["q8_24"], label="my_expm1f Q8.24")
    plt.xlabel("x, near zero")
    plt.ylabel("expm1f(x)")
    plt.title("Q8.24 vs libm near zero")
    plt.grid(True)
    plt.legend()
    save_plot(os.path.join(OUT_DIR, "q8_vs_libm_near_zero.png"))

    # ------------------------------------------------------------
    # 8. Zoom in near zero: absolute error
    # ------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    plt.plot(df_zoom["x"], df_zoom["q8_abs_err"], label="Q8.24 absolute error")
    plt.xlabel("x, near zero")
    plt.ylabel("absolute error: |Q8.24 - libm|")
    plt.title("Near-zero absolute error of Q8.24 Taylor approximation")
    plt.grid(True)
    plt.legend()
    save_plot(os.path.join(OUT_DIR, "q8_near_zero_absolute_error.png"))

    print(f"\nFigures and table were written to: {OUT_DIR}")


if __name__ == "__main__":
    main()
