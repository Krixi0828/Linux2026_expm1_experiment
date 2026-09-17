#!/usr/bin/env python3
from pathlib import Path
import argparse
import math
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


def safe_rmse(s):
    a = pd.to_numeric(s, errors="coerce").dropna().to_numpy(dtype=float)
    return float(np.sqrt(np.mean(a * a))) if len(a) else math.nan


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--accuracy", default="results/accuracy.csv")
    ap.add_argument("--timing", default="results/timing.csv")
    ap.add_argument("--outdir", default="results")
    ap.add_argument("--plotdir", default="plots")
    args = ap.parse_args()

    outdir = Path(args.outdir)
    plotdir = Path(args.plotdir)
    outdir.mkdir(parents=True, exist_ok=True)
    plotdir.mkdir(parents=True, exist_ok=True)

    df = pd.read_csv(args.accuracy)
    numeric_cols = [
        "x", "approx", "reference", "signed_error", "abs_error", "rel_error",
        "terms", "muls", "divs", "shifts", "adds", "saturations",
        "max_abs_intermediate_real", "reduction_steps"
    ]
    for c in numeric_cols:
        df[c] = pd.to_numeric(df[c], errors="coerce")

    rows = []
    for method, g in df.groupby("method", sort=False):
        rel = g["rel_error"].replace([np.inf, -np.inf], np.nan).dropna()
        rows.append({
            "method": method,
            "max_abs_error": g["abs_error"].max(),
            "mean_abs_error": g["abs_error"].mean(),
            "rmse": safe_rmse(g["signed_error"]),
            "max_rel_error": rel.max() if len(rel) else np.nan,
            "mean_rel_error": rel.mean() if len(rel) else np.nan,
            "mean_terms": g["terms"].mean(),
            "mean_muls": g["muls"].mean(),
            "mean_divs": g["divs"].mean(),
            "mean_shifts": g["shifts"].mean(),
            "mean_adds": g["adds"].mean(),
            "total_saturations": g["saturations"].sum(),
            "saturation_point_rate": (g["saturations"] > 0).mean(),
            "max_intermediate_abs": g["max_abs_intermediate_real"].max(),
            "mean_reduction_steps": g["reduction_steps"].mean(),
        })

    summary = pd.DataFrame(rows)

    timing_path = Path(args.timing)
    if timing_path.exists() and timing_path.stat().st_size:
        timing = pd.read_csv(timing_path)
        timing["ns_per_call"] = pd.to_numeric(timing["ns_per_call"], errors="coerce")
        timing_summary = (timing.groupby(["method", "scope"])["ns_per_call"]
                          .agg(ns_per_call_mean="mean", ns_per_call_std="std")
                          .reset_index())
        timing_summary.to_csv(outdir / "timing_summary.csv", index=False)

        kernel = timing_summary[timing_summary["scope"] == "kernel"][["method", "ns_per_call_mean", "ns_per_call_std"]]
        kernel = kernel.rename(columns={
            "ns_per_call_mean": "kernel_ns_per_call",
            "ns_per_call_std": "kernel_ns_std",
        })
        e2e = timing_summary[timing_summary["scope"] == "e2e"][["method", "ns_per_call_mean", "ns_per_call_std"]]
        e2e = e2e.rename(columns={
            "ns_per_call_mean": "e2e_ns_per_call",
            "ns_per_call_std": "e2e_ns_std",
        })
        summary = summary.merge(kernel, on="method", how="left").merge(e2e, on="method", how="left")

    summary.to_csv(outdir / "summary.csv", index=False)

    # Region-specific summaries: useful for near-zero vs large-negative behavior.
    regions = {
        "full": lambda g: g,
        "near_zero_abs_le_0.05": lambda g: g[g["x"].abs() <= 0.05],
        "core_abs_le_1": lambda g: g[g["x"].abs() <= 1.0],
        "negative_lt_minus_1": lambda g: g[g["x"] < -1.0],
        "positive_gt_0": lambda g: g[g["x"] > 0.0],
    }
    region_rows = []
    for method, mg in df.groupby("method", sort=False):
        for region_name, selector in regions.items():
            g = selector(mg)
            if g.empty:
                continue
            region_rows.append({
                "method": method,
                "region": region_name,
                "points": len(g),
                "max_abs_error": g["abs_error"].max(),
                "mean_abs_error": g["abs_error"].mean(),
                "rmse": safe_rmse(g["signed_error"]),
                "saturation_point_rate": (g["saturations"] > 0).mean(),
                "max_intermediate_abs": g["max_abs_intermediate_real"].max(),
                "mean_terms": g["terms"].mean(),
            })
    pd.DataFrame(region_rows).to_csv(outdir / "region_summary.csv", index=False)

    # Markdown table for direct use in the report.
    cols = ["method", "max_abs_error", "mean_abs_error", "rmse",
            "saturation_point_rate", "max_intermediate_abs", "mean_terms"]
    if "kernel_ns_per_call" in summary.columns:
        cols.append("kernel_ns_per_call")
    if "e2e_ns_per_call" in summary.columns:
        cols.append("e2e_ns_per_call")
    md = summary[cols].copy()
    for c in md.columns:
        if c != "method":
            md[c] = md[c].map(lambda x: "" if pd.isna(x) else f"{x:.6g}")
    (outdir / "summary.md").write_text(md.to_markdown(index=False), encoding="utf-8")

    # Plot 1: absolute error curves, downsample for readability.
    selected = [
        "direct_t12_trunc", "direct_t20_rne",
        "rr_ln2_t12", "halving_t12",
        "fine_ln2_8_t12", "fine_ln2_64_t12"
    ]
    plt.figure(figsize=(10, 6))
    for method in selected:
        g = df[df["method"] == method]
        if g.empty:
            continue
        stride = max(1, len(g) // 4000)
        gx = g.iloc[::stride]
        y = gx["abs_error"].clip(lower=1e-16)
        plt.semilogy(gx["x"], y, label=method)
    plt.xlabel("x")
    plt.ylabel("absolute error")
    plt.title("Fixed-point expm1: absolute error")
    plt.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(plotdir / "abs_error_vs_x.png", dpi=180)
    plt.close()

    # Plot 2: accuracy-cost trade-off.
    if "kernel_ns_per_call" in summary.columns:
        p = summary.dropna(subset=["kernel_ns_per_call", "max_abs_error"])
        plt.figure(figsize=(9, 6))
        for _, r in p.iterrows():
            plt.scatter(r["kernel_ns_per_call"], max(r["max_abs_error"], 1e-16))
            plt.annotate(r["method"], (r["kernel_ns_per_call"], max(r["max_abs_error"], 1e-16)), fontsize=7)
        plt.yscale("log")
        plt.xlabel("kernel ns / call")
        plt.ylabel("maximum absolute error")
        plt.title("Accuracy–performance trade-off")
        plt.tight_layout()
        plt.savefig(plotdir / "accuracy_vs_performance.png", dpi=180)
        plt.close()

    # Plot 3: range robustness.
    p = summary.sort_values("max_intermediate_abs")
    plt.figure(figsize=(10, 6))
    x = np.arange(len(p))
    plt.bar(x, p["max_intermediate_abs"])
    plt.axhline(128.0, linestyle="--", linewidth=1.0, label="Q8.24 magnitude limit (128)")
    plt.xticks(x, p["method"], rotation=60, ha="right", fontsize=8)
    plt.ylabel("peak pre-saturation |intermediate| (real units)")
    plt.title("Peak pre-saturation intermediate magnitude")
    plt.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(plotdir / "max_intermediate.png", dpi=180)
    plt.close()


    # Plot 4: near-zero accuracy.
    nz = df[df["x"].abs() <= 0.05]
    if not nz.empty:
        plt.figure(figsize=(10, 6))
        for method in selected:
            g = nz[nz["method"] == method]
            if g.empty:
                continue
            plt.semilogy(g["x"], g["abs_error"].clip(lower=1e-16), label=method)
        plt.xlabel("x")
        plt.ylabel("absolute error")
        plt.title("Near-zero absolute error (|x| <= 0.05)")
        plt.legend(fontsize=8)
        plt.tight_layout()
        plt.savefig(plotdir / "near_zero_abs_error.png", dpi=180)
        plt.close()

    # Plot 5: large-negative behavior.
    neg = df[df["x"] <= -1.0]
    if not neg.empty:
        plt.figure(figsize=(10, 6))
        for method in selected:
            g = neg[neg["method"] == method]
            if g.empty:
                continue
            stride = max(1, len(g) // 3000)
            gx = g.iloc[::stride]
            plt.semilogy(gx["x"], gx["abs_error"].clip(lower=1e-16), label=method)
        plt.xlabel("x")
        plt.ylabel("absolute error")
        plt.title("Large-negative-domain absolute error")
        plt.legend(fontsize=8)
        plt.tight_layout()
        plt.savefig(plotdir / "negative_domain_abs_error.png", dpi=180)
        plt.close()

    # Fine-grain ln2/N sweep summary and plots.
    fine_rows = []
    method_to_n = {
        "rr_ln2_t12": 1,
        "fine_ln2_2_t12": 2,
        "fine_ln2_4_t12": 4,
        "fine_ln2_8_t12": 8,
        "fine_ln2_16_t12": 16,
        "fine_ln2_32_t12": 32,
        "fine_ln2_64_t12": 64,
    }
    for method, N in method_to_n.items():
        hit = summary[summary["method"] == method]
        if hit.empty:
            continue
        row = hit.iloc[0]
        fine_rows.append({
            "N": N,
            "method": method,
            "max_abs_error": row["max_abs_error"],
            "mean_abs_error": row["mean_abs_error"],
            "rmse": row["rmse"],
            "mean_terms": row["mean_terms"],
            "saturation_point_rate": row["saturation_point_rate"],
            "kernel_ns_per_call": row.get("kernel_ns_per_call", np.nan),
            "e2e_ns_per_call": row.get("e2e_ns_per_call", np.nan),
        })
    fine = pd.DataFrame(fine_rows).sort_values("N") if fine_rows else pd.DataFrame()
    if not fine.empty:
        fine.to_csv(outdir / "fine_grain_sweep.csv", index=False)

        plt.figure(figsize=(8, 5))
        plt.plot(fine["N"], fine["max_abs_error"], marker="o")
        plt.xscale("log", base=2)
        plt.yscale("log")
        plt.xticks(fine["N"], [str(int(n)) for n in fine["N"]])
        plt.xlabel("N in reduction step ln2/N")
        plt.ylabel("maximum absolute error")
        plt.title("Fine-grain reduction: error vs N")
        plt.tight_layout()
        plt.savefig(plotdir / "fine_grain_max_error_vs_N.png", dpi=180)
        plt.close()

        if fine["kernel_ns_per_call"].notna().any():
            plt.figure(figsize=(8, 5))
            plt.plot(fine["N"], fine["kernel_ns_per_call"], marker="o")
            plt.xscale("log", base=2)
            plt.xticks(fine["N"], [str(int(n)) for n in fine["N"]])
            plt.xlabel("N in reduction step ln2/N")
            plt.ylabel("kernel ns / call")
            plt.title("Fine-grain reduction: latency vs N")
            plt.tight_layout()
            plt.savefig(plotdir / "fine_grain_latency_vs_N.png", dpi=180)
            plt.close()

        plt.figure(figsize=(8, 5))
        plt.plot(fine["N"], fine["mean_terms"], marker="o")
        plt.xscale("log", base=2)
        plt.xticks(fine["N"], [str(int(n)) for n in fine["N"]])
        plt.xlabel("N in reduction step ln2/N")
        plt.ylabel("mean Taylor terms")
        plt.title("Fine-grain reduction: Taylor work vs N")
        plt.tight_layout()
        plt.savefig(plotdir / "fine_grain_terms_vs_N.png", dpi=180)
        plt.close()

    # Auto-generated concise findings, without inventing claims beyond the run.
    lines = ["# Auto-generated findings", ""]
    if len(summary):
        acc_row = summary.loc[summary["max_abs_error"].idxmin()]
        lines.append(f"- Lowest measured maximum absolute error: `{acc_row['method']}` = {acc_row['max_abs_error']:.6g}.")
        sat_row = summary.loc[summary["saturation_point_rate"].idxmin()]
        lines.append(f"- Lowest saturation-point rate: `{sat_row['method']}` = {sat_row['saturation_point_rate']:.6g}.")
        if "kernel_ns_per_call" in summary.columns:
            fixed = summary.dropna(subset=["kernel_ns_per_call"])
            fixed = fixed[fixed["method"] != "libm"]
            if len(fixed):
                speed_row = fixed.loc[fixed["kernel_ns_per_call"].idxmin()]
                lines.append(f"- Lowest measured fixed-point kernel latency: `{speed_row['method']}` = {speed_row['kernel_ns_per_call']:.6g} ns/call.")
    lines.append("")
    lines.append("Use `summary.csv` for the full Accuracy–Range–Cost comparison.")
    (outdir / "findings.md").write_text("\n".join(lines), encoding="utf-8")

    print(summary.to_string(index=False))
    print(f"\nWrote {outdir/'summary.csv'}, {outdir/'summary.md'}, {outdir/'findings.md'}")
    print(f"Plots are in {plotdir}")


if __name__ == "__main__":
    main()
