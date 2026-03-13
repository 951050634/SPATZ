// Input/output logic consistent with dp-faxpy / dp-fmatmul

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/sp-cmatmul.c"

cfloat *a;
cfloat *b;
cfloat *c;

static inline int cp_check(const cfloat calc, const cfloat ref) {
  const float th = 0.0001f;
  float dr = calc.real - ref.real, di = calc.imag - ref.imag;
  if (dr < 0) dr = -dr;
  if (di < 0) di = -di;
  return (dr > th) || (di > th);
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  unsigned int timer = (unsigned int)-1;
  const unsigned int kernel_size = 4;
  const unsigned int M = cmatmul_M, N = cmatmul_N, P = cmatmul_P;

  if (cid == 0) {
    a = (cfloat *)snrt_l1alloc(M * N * sizeof(cfloat));
    b = (cfloat *)snrt_l1alloc(N * P * sizeof(cfloat));
    c = (cfloat *)snrt_l1alloc(M * P * sizeof(cfloat));
  }
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    snrt_dma_start_1d(a, cmatmul_a, M * N * sizeof(cfloat));
    snrt_dma_start_1d(b, cmatmul_b, N * P * sizeof(cfloat));
    snrt_dma_start_1d(c, cmatmul_c, M * P * sizeof(cfloat));
    snrt_dma_wait_all();
  }
  snrt_cluster_hw_barrier();

  unsigned int m_start = (M / num_cores) * cid;
  unsigned int m_end = (M / num_cores) * (cid + 1);

  unsigned int t_start = benchmark_get_cycle();
  if (cid == 0) start_kernel();
  if (kernel_size == 2)
    matmul_2xVL(c, a, b, m_start, m_end, N, P, 0, P);
  else
    matmul_4xVL(c, a, b, m_start, m_end, N, P, 0, P);
  snrt_cluster_hw_barrier();
  if (cid == 0) stop_kernel();
  timer = benchmark_get_cycle() - t_start;

  if (cid == 0) {
    PRINTF("\n----- (%u x %u) sp cmatmul -----\n", M, P);
    PRINTF("The execution took %u cycles.\n", timer);
  }
  if (cid == 0) {
    int err = 0;
    for (unsigned int i = 0; i < M * P; ++i)
      if (cp_check(c[i], cmatmul_result[i])) err++;
    if (err) return -1;
    PRINTF("Verification: SUCCESS\n");
  }
  snrt_cluster_hw_barrier();
  return 0;
}
