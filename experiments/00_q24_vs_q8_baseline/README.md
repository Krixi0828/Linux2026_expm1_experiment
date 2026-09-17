# `expm1` Fixed-Point Experiment

本專案實作並比較 `expm1f(x) = e^x - 1` 的不同定點數版本，重點探討定點數格式在精度、可表示範圍與量化誤差之間的取捨。

## 研究目標

對於接近零的輸入，直接計算：

$$
e^x - 1
$$

可能因為浮點數相減而失去有效位元，因此標準函式庫提供 `expm1` / `expm1f` 專門處理此問題。

本實驗以定點數方式實作 `expm1f`，比較兩種格式：

* **Q24.8**：整數部分 24 bits、小數部分 8 bits。
* **Q8.24**：整數部分 8 bits、小數部分 24 bits。

主要觀察兩種格式在靠近 $x = 0$ 時的解析度差異，以及其相對於 `libm expm1f()` 的誤差表現。

## 實驗結論摘要

* Q24.8 的最小解析度為：

  $$
  2^{-8} = \frac{1}{256}
  $$

  因此當輸入非常接近零時，數值可能直接量化為零，造成明顯誤差。

* Q8.24 的最小解析度為：

  $$
  2^{-24} \approx 5.96 \times 10^{-8}
  $$

  能保留更多接近零的低位資訊，因此在 near-zero 範圍內與 `libm expm1f()` 的結果更接近。

* Q24.8 擁有較大的整數表示範圍，但犧牲了小數精度。

* Q8.24 較適合本實驗中重視 near-zero 精度的 `expm1f` 計算情境。

## 檔案說明

| 檔案 / 資料夾                    | 說明                               |
| --------------------------- | -------------------------------- |
| `my_expm1f_q24_8.c`         | Q24.8 定點數版本的 `expm1f` 實作。        |
| `my_expm1f_q8_24.c`         | Q8.24 定點數版本的 `expm1f` 實作。        |
| `generate_expm1_data.c`     | 產生測試資料，並輸出定點數版本與 `libm` 比較結果。    |
| `expm1_compare.csv`         | 實驗輸出的原始比較資料。                     |
| `compare_q24_q8_vs_libm.py` | 同時比較 Q24.8、Q8.24 與 `libm` 的繪圖程式。 |
| `plot_q24_vs_libm.py`       | 繪製 Q24.8 與 `libm` 的比較圖。          |
| `plot_q8_vs_libm.py`        | 繪製 Q8.24 與 `libm` 的比較圖。          |
| `q24_q8_compare_results/`   | Q24.8 與 Q8.24 的比較圖與誤差摘要。         |
| `q24_vs_libm_results/`      | Q24.8 相對於 `libm` 的比較結果。          |
| `q8_vs_libm_results/`       | Q8.24 相對於 `libm` 的比較結果。          |

## 產生的比較圖

本專案包含下列類型的圖表：

* value comparison
* signed error
* absolute error
* logarithmic absolute error
* relative error
* near-zero value comparison
* near-zero absolute error comparison

這些圖用於觀察不同定點格式在整體輸入範圍與 $x \approx 0$ 區域的誤差特性。

## 相關筆記

* [libm 中 ](https://hackmd.io/@sysprog/rkP0B-Offl)[`expm1`](https://hackmd.io/@sysprog/rkP0B-Offl)[ 的實作與數值分析](https://hackmd.io/@sysprog/rkP0B-Offl)

