// Input/output logic consistent with dp-faxpy / dp-fmatmul

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/dp-cmatmul.c"

cdouble *a;
cdouble *b;
cdouble *c;

static inline int cp_check(const cdouble calc, const cdouble ref) {
  const double th = 0.0001;
  double dr = calc.real - ref.real, di = calc.imag - ref.imag;
  if (dr < 0) dr = -dr;
  if (di < 0) di = -di;
  return (dr > th) || (di > th);
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int measure_iterations = 1;
  unsigned int timer = (unsigned int)-1;
  const unsigned int kernel_size = 4;

  const unsigned int M = cmatmul_M, N = cmatmul_N, P = cmatmul_P;

  if (cid == 0) {
    a = (cdouble *)snrt_l1alloc(M * N * sizeof(cdouble));
    b = (cdouble *)snrt_l1alloc(N * P * sizeof(cdouble));
    c = (cdouble *)snrt_l1alloc(M * P * sizeof(cdouble));
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    snrt_dma_start_1d(a, cmatmul_a, M * N * sizeof(cdouble));
    snrt_dma_start_1d(b, cmatmul_b, N * P * sizeof(cdouble));
    snrt_dma_start_1d(c, cmatmul_c, M * P * sizeof(cdouble));
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  unsigned int m_start = (M / num_cores) * cid;
  unsigned int m_end = (M / num_cores) * (cid + 1);

  for (unsigned int i = 0; i < measure_iterations; ++i) {
    unsigned int t_start = benchmark_get_cycle();
    if (cid == 0) start_kernel();

    if (kernel_size == 2)
      cmatmul_2xVL(c, a, b, m_start, m_end, N, P, 0, P);
    else if (kernel_size == 4)
      cmatmul_4xVL(c, a, b, m_start, m_end, N, P, 0, P);
    else
      return -2;

    snrt_cluster_hw_barrier();
    if (cid == 0) stop_kernel();
    unsigned int t_end = benchmark_get_cycle();
    if (cid == 0 && (t_end - t_start) < timer) timer = t_end - t_start;
  }

  if (cid == 0) {
    long unsigned int perf = 1000 * 2 * 2 * M * P * N / timer;
    PRINTF("\n----- (%ux%u) dp cmatmul -----\n", M, P);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %lu OP/1000cycle.\n", perf);
  }

  if (cid == 0) {
    int err = 0;
    for (unsigned int i = 0; i < M * P; ++i)
      if (cp_check(c[i], cmatmul_result[i])) {
        if (err < 5) PRINTF("Error: c[%u] = (%f,%f), expected (%f,%f)\n", i,
            (float)c[i].real, (float)c[i].imag,
            (float)cmatmul_result[i].real, (float)cmatmul_result[i].imag);
        err++;
      }
    if (err) return -1;
    PRINTF("Verification: SUCCESS\n");
  }

  snrt_cluster_hw_barrier();
  return 0;
}
