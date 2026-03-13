// Copyright 2022 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "../dp-cmatmul/kernel/dp-cmatmul.c"

#define CMATMUL_TILE_M 16

cdouble *a;
cdouble *b;
cdouble *c;

static inline int cp_check(const cdouble calc, const cdouble ref)
{
    const double th = 0.0001;
    double dr = calc.real - ref.real, di = calc.imag - ref.imag;
    if (dr < 0)
        dr = -dr;
    if (di < 0)
        di = -di;
    return (dr > th) || (di > th);
}

int main()
{
    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();

    const unsigned int M = cmatmul_M;
    const unsigned int N = cmatmul_N;
    const unsigned int P = cmatmul_P;

    unsigned int timer = (unsigned int)-1;

    if (cid == 0)
    {
        a = (cdouble *)snrt_l1alloc(CMATMUL_TILE_M * N * sizeof(cdouble));
        b = (cdouble *)snrt_l1alloc(N * P * sizeof(cdouble));
        c = (cdouble *)snrt_l1alloc(CMATMUL_TILE_M * P * sizeof(cdouble));
    }

    snrt_cluster_hw_barrier();

    if (cid == 0)
    {
        snrt_dma_start_1d(b, cmatmul_b, N * P * sizeof(cdouble));
        snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();

    if (cid == 0)
    {
        start_kernel();
        timer = benchmark_get_cycle();
    }

    int total_err = 0;

    for (unsigned int m_off = 0; m_off < M; m_off += CMATMUL_TILE_M)
    {
        unsigned int tile_m = M - m_off;
        if (tile_m > CMATMUL_TILE_M)
            tile_m = CMATMUL_TILE_M;

        if (cid == 0)
        {
            snrt_dma_start_1d(a, cmatmul_a + m_off * N, tile_m * N * sizeof(cdouble));
            snrt_dma_start_1d(c, cmatmul_c + m_off * P, tile_m * P * sizeof(cdouble));
            snrt_dma_wait_all();
        }

        snrt_cluster_hw_barrier();

        unsigned int m_start = (tile_m * cid) / num_cores;
        unsigned int m_end = (tile_m * (cid + 1)) / num_cores;
        if (m_end > m_start)
        {
            cmatmul_4xVL(c, a, b, m_start, m_end, N, P, 0, P);
        }

        snrt_cluster_hw_barrier();

        if (cid == 0)
        {
            for (unsigned int i = 0; i < tile_m * P; ++i)
            {
                if (cp_check(c[i], cmatmul_result[m_off * P + i]))
                {
                    if (total_err < 5)
                    {
                        PRINTF("Error: c[%u] = (%f,%f), expected (%f,%f)\n",
                               m_off * P + i, (float)c[i].real, (float)c[i].imag,
                               (float)cmatmul_result[m_off * P + i].real,
                               (float)cmatmul_result[m_off * P + i].imag);
                    }
                    total_err++;
                }
            }
        }

        snrt_cluster_hw_barrier();
    }

    if (cid == 0)
    {
        stop_kernel();
        timer = benchmark_get_cycle() - timer;

        long unsigned int perf = 1000 * 2 * 2 * M * P * N / timer;
        PRINTF("\n----- (%ux%u) dp cmatmul ver2 -----\n", M, P);
        PRINTF("The execution took %u cycles.\n", timer);
        PRINTF("The performance is %lu OP/1000cycle.\n", perf);

        if (total_err)
            return -1;
        PRINTF("Verification: SUCCESS\n");
    }

    snrt_cluster_hw_barrier();
    return 0;
}
