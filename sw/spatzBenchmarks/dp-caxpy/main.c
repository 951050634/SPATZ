#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

// #include "caxpy.h" 

// 包含生成的数据头文件
#include DATAHEADER

// 如果是把 caxpy_v64b 的实现放在 .c 里且没在 CMakeLists 里单独编译，
// 这里可能需要 include .c，或者通常只 include .h 并在链接时处理。
// 这里参考 faxpy 的写法，如果 faxpy 直接 include 了 .c，这里也保留注释
#include "kernel/caxpy.c"

// 定义全局指针 (L1 Memory)
cdouble *alpha_ptr;
cdouble *x;
cdouble *y;

// 复数误差检查
static inline int cp_check(const cdouble calc, const cdouble ref) {
  const double threshold = 0.0001;

  double diff_real = calc.real - ref.real;
  double diff_imag = calc.imag - ref.imag;

  if (diff_real < 0) diff_real = -diff_real;
  if (diff_imag < 0) diff_imag = -diff_imag;

  return (diff_real > threshold) || (diff_imag > threshold);
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  // caxpy_n 来自 gen-data.py 生成的头文件
  const unsigned int dim = caxpy_n;
  const unsigned int dim_core = dim / num_cores;

  // Allocate the vectors in L1 memory
  if (cid == 0) {
    // 标量 alpha
    alpha_ptr = (cdouble *)snrt_l1alloc(sizeof(cdouble));
    // 向量 X, Y
    x = (cdouble *)snrt_l1alloc(dim * sizeof(cdouble));
    y = (cdouble *)snrt_l1alloc(dim * sizeof(cdouble));
  }

  // Initialize the matrices (DMA Transfer from DRAM to L1)
  if (cid == 0) {
    *alpha_ptr = caxpy_alpha; // 结构体赋值

    // 传输数据，注意 size 要乘 sizeof(cdouble)
    snrt_dma_start_1d(x, caxpy_x, dim * sizeof(cdouble));
    snrt_dma_start_1d(y, caxpy_y, dim * sizeof(cdouble));
  }

  // Wait for all cores to finish DMA setup
  snrt_cluster_hw_barrier();

  // Calculate internal pointers for this core
  cdouble *x_int = x + dim_core * cid;
  cdouble *y_int = y + dim_core * cid;

  // Wait for all cores to sync before starting kernel
  snrt_cluster_hw_barrier();

  // Start dump (Simulation trace)
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();

  // Call CAXPY Kernel
  // 注意：alpha_ptr 是指针，我们解引用传值；x_int/y_int 是指针
  caxpy_v64b(*alpha_ptr, x_int, y_int, dim_core);

  // Wait for all cores to finish computation
  snrt_cluster_hw_barrier();

  // End timer
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  // End dump
  if (cid == 0)
    stop_kernel();

  // Check and display results
  if (cid == 0) {
    // 复数运算量计算：
    // y[i] = alpha * x[i] + y[i]
    // 1个复数乘法 = 4 FLOPs (2 mul, 1 add, 1 sub)
    // 1个复数加法 = 2 FLOPs (2 add)
    // 标量乘法如果是复数 = 2 FLOPs (alpha * x_real, alpha * x_imag) - 这里假设全复数运算
    // 总计：每个元素 8 FLOPs (4 mul, 4 add/sub)
    // 如果硬件有 Fused 指令，Cycle 数会减少，但 FLOPs 定义不变
    long unsigned int ops_per_elem = 8;
    
    long unsigned int performance = 1000 * ops_per_elem * dim / timer;
    long unsigned int utilization =
        performance / (ops_per_elem * num_cores * SNRT_NFPU_PER_CORE); // 粗略估算

    PRINTF("\n----- (%d) caxpy -----\n", dim);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle.\n", performance);
  }

  // Verification
  if (cid == 0) {
    int error_count = 0;
    for (unsigned int i = 0; i < dim; i++) {
      if (cp_check(y[i], caxpy_result[i])) {
        error_count++;
        // 限制打印数量防止刷屏
        if (error_count <= 10) {
            PRINTF("Error: Index %d -> Result = (%f, %f), Expected = (%f, %f)\n", 
                   i, 
                   (float)y[i].real, (float)y[i].imag, 
                   (float)caxpy_result[i].real, (float)caxpy_result[i].imag);
        }
      }
    }
    if (error_count == 0) {
        PRINTF("Verification: SUCCESS\n");
    } else {
        PRINTF("Verification: FAILED (%d errors)\n", error_count);
    }
  }

  // Wait for core 0 to finish
  snrt_cluster_hw_barrier();

  return 0;
}