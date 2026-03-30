// Copyright 2026 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "../dp-fdotp/kernel/fdotp.c"

#define FDOTP_TILE_ELEMS 1024

double *a;
double *b;
double *result;

static inline int fp_check(const double calc, const double ref)
{
    const double threshold = 0.00001;
    double diff = calc - ref;
    if (diff < 0)
        diff = -diff;
    return diff > threshold;
}

int main()
{
    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();
    const unsigned int dim = dotp_l.M;

    unsigned int timer = (unsigned int)-1;

    if (cid == 0)
    {
        a = (double *)snrt_l1alloc(FDOTP_TILE_ELEMS * sizeof(double));
        b = (double *)snrt_l1alloc(FDOTP_TILE_ELEMS * sizeof(double));
        result = (double *)snrt_l1alloc(num_cores * sizeof(double));
    }

    snrt_cluster_hw_barrier();

    if (cid == 0)
        start_kernel();
    if (cid == 0)
        timer = benchmark_get_cycle();

    double acc = 0.0;

    for (unsigned int offset = 0; offset < dim; offset += FDOTP_TILE_ELEMS)
    {
        unsigned int tile_elems = dim - offset;
        if (tile_elems > FDOTP_TILE_ELEMS)
            tile_elems = FDOTP_TILE_ELEMS;

        if (cid == 0)
        {
            snrt_dma_start_1d(a, dotp_A_dram + offset, tile_elems * sizeof(double));
            snrt_dma_start_1d(b, dotp_B_dram + offset, tile_elems * sizeof(double));
            snrt_dma_wait_all();
        }

        snrt_cluster_hw_barrier();

        unsigned int start = (tile_elems * cid) / num_cores;
        unsigned int end = (tile_elems * (cid + 1)) / num_cores;
        if (end > start)
        {
            acc += fdotp_v64b(a + start, b + start, end - start);
        }

        snrt_cluster_hw_barrier();
    }

    result[cid] = acc;

    snrt_cluster_hw_barrier();

    if (cid == 0)
    {
        double sum = result[0];
        for (unsigned int i = 1; i < num_cores; ++i)
            sum += result[i];
        result[0] = sum;
    }

    snrt_cluster_hw_barrier();

    if (cid == 0)
        stop_kernel();
    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    if (cid == 0)
    {
        long unsigned int performance = 1000 * 2 * dim / timer;
        PRINTF("\n----- (%d) dp fdotp aligned -----\n", dim);
        PRINTF("The execution took %u cycles.\n", timer);
        PRINTF("The performance is %ld OP/1000cycle.\n", performance);

        if (fp_check(result[0], dotp_result))
        {
            PRINTF("Error: Result = %f, Golden = %f\n", result[0], dotp_result);
            return -1;
        }
        PRINTF("Verification: SUCCESS\n");
    }

    snrt_cluster_hw_barrier();
    return 0;
}
