# 复数 (f->c) 测试项目说明

以下为 f->c 复数测试项目，与 dp-faxpy 的输入输出逻辑一致：DMA 搬入数据、多核分块、调用 kernel、与 golden 比对验证。

---

## 一、已完成工作概览

### 1. 各项目的 gen_data 脚本（第一步）

除 **dp-cmatmul** 外，每个复数项目都有**独立、自包含**的 `script/gen_data.py`，只负责本项目的测试数据生成，输出到本项目下的 `data/` 目录：

| 项目 | 脚本职责 | 输出文件命名 |
|------|----------|--------------|
| dp-caxpy | 仅 CAXPY（y = alpha*x + y） | `data_${M}.h` |
| dp-cdotp | 仅 CDOTP（result = sum(x*y)） | `data_${M}.h` |
| dp-cmatmul | 仅 CMATMUL（C += A*B），已有脚本 | `data_${M}_${N}_${K}.h` |
| sp-cmatmul | 仅 CMATMUL（cfloat） | `data_${M}_${N}_${K}.h` |
| hp-cmatmul | 仅 CMATMUL（chalf） | `data_${M}_${N}_${K}.h` |
| widening-hp-cmatmul | 仅 CMATMUL（chalf） | `data_${M}_${N}_${K}.h` |
| widening-bp-cmatmul | 仅 CMATMUL（cbyte，整数复数） | `data_${M}_${N}_${K}.h` |

依赖说明：除 **widening-bp-cmatmul** 仅需 numpy + hjson 外，其余脚本均需 Python3 + numpy + torch + hjson。这些库已在 **spatz 项目虚拟环境 venv** 中安装，生成数据时请先激活 venv。

### 2. 激活 venv 并生成 data（第二步）

构建与运行测试前，需要先**激活项目虚拟环境**，再在各项目的 `script/` 下执行对应的 `gen_data.py`，将生成的头文件写入各项目下的 `data/` 目录。已按此流程为所有复数项目生成过一轮数据，生成的文件与 CMakeLists 中的 `DATAHEADER` 命名一致，可直接参与构建。

---

## 二、项目列表与配置

| 项目 | 功能 | 精度 | script 配置 | 生成 data 示例 |
|------|------|------|-------------|----------------|
| dp-caxpy | 复数 AXPY | cdouble | `caxpy_256.hjson`, `caxpy.hjson`(1024) | `data_256.h`, `data_1024.h` |
| dp-cdotp | 复数点积 | cdouble | `cdotp.hjson`, `cdotp_4096.hjson` | `data_128.h`, `data_4096.h` |
| dp-cmatmul | 复数矩阵乘 | cdouble | `matmul.hjson` | `data_64_64_64.h` |
| sp-cmatmul | 复数矩阵乘 | cfloat | `matmul.hjson`, `matmul_64_128_64.hjson` | `data_64_64_64.h`, `data_64_128_64.h` |
| hp-cmatmul | 复数矩阵乘 | chalf | `matmul.hjson`, `matmul_64_128_64.hjson`, `matmul_128_128_128.hjson` | 同上尺寸 |
| widening-hp-cmatmul | 复数矩阵乘 | chalf | 同上 | 同上 |
| widening-bp-cmatmul | 复数矩阵乘 | cbyte | `matmul.hjson`, `matmul_64_128_64.hjson`, `matmul_128_128_128.hjson`, `matmul_128_256_128.hjson` | 同上尺寸 |

---

## 三、如何生成 data 目录（推荐流程）

1. **激活虚拟环境**（库仅在 spatz 项目 venv 中安装时，必须执行）：
   ```bash
   source /path/to/spatz/venv/bin/activate
   ```
2. **进入各项目 `script/` 目录**，用对应 `.hjson` 调用 `gen_data.py`，生成的文件会写入**当前项目**的 `data/` 目录。

以下命令均假设已激活 venv，且从 `sw/spatzBenchmarks/` 下执行：

```bash
# dp-caxpy
cd dp-caxpy/script && python3 gen_data.py -c caxpy_256.hjson && python3 gen_data.py -c caxpy.hjson

# dp-cdotp
cd dp-cdotp/script && python3 gen_data.py -c cdotp.hjson && python3 gen_data.py -c cdotp_4096.hjson

# dp-cmatmul
cd dp-cmatmul/script && python3 gen_data.py -c matmul.hjson

# sp-cmatmul
cd sp-cmatmul/script && python3 gen_data.py -c matmul.hjson && python3 gen_data.py -c matmul_64_128_64.hjson

# hp-cmatmul
cd hp-cmatmul/script && python3 gen_data.py -c matmul.hjson && python3 gen_data.py -c matmul_64_128_64.hjson && python3 gen_data.py -c matmul_128_128_128.hjson

# widening-hp-cmatmul
cd widening-hp-cmatmul/script && python3 gen_data.py -c matmul.hjson && python3 gen_data.py -c matmul_64_128_64.hjson && python3 gen_data.py -c matmul_128_128_128.hjson

# widening-bp-cmatmul
cd widening-bp-cmatmul/script && python3 gen_data.py -c matmul.hjson && python3 gen_data.py -c matmul_64_128_64.hjson && python3 gen_data.py -c matmul_128_128_128.hjson && python3 gen_data.py -c matmul_128_256_128.hjson
```

生成的头文件（如 `data/data_64_64_64.h`）与 CMakeLists 中的 `DATAHEADER="data/data_${M}_${N}_${K}.h"`（或单参数 `data/data_${M}.h`）对应，可直接用于构建与测试。
