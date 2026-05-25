// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <debug.h>
#include <perf_cnt.h>
#include <snrt.h>
#include <spatz_cluster_peripheral.h>
#include <stdint.h>
#include <stdio.h>

#undef PRINTF
#define PRINTF(...) printf(__VA_ARGS__)

#define MAX_N 16
#define MAX_D 64
#define TOL 1.0e-4f

typedef struct {
  float m_old[MAX_N];
  float l_old[MAX_N];
  float o_old[MAX_N][MAX_D];
  float m_tile[MAX_N];
  float l_tile[MAX_N];
  float o_tile[MAX_N][MAX_D];
  float m_out[MAX_N];
  float l_out[MAX_N];
  float o_out[MAX_N][MAX_D];
  float o_old_packed[MAX_N * MAX_D];
  float o_tile_packed[MAX_N * MAX_D];
  float o_out_packed[MAX_N * MAX_D];
  float m_ref[MAX_N];
  float l_ref[MAX_N];
  float o_ref[MAX_N][MAX_D];
} smu_buffers_t;

static smu_buffers_t *buf;

static volatile uint32_t *cluster_reg(uint32_t off) {
  return (volatile uint32_t *)(snrt_cluster_memory().end + off);
}

static uint32_t tcdm_off(const void *ptr) {
  return (uint32_t)((uintptr_t)ptr - (uintptr_t)snrt_cluster_memory().start);
}

static float absf(float x) { return x < 0.0f ? -x : x; }

static int fp_close(float got, float exp) {
  float diff = absf(got - exp);
  float scale = absf(exp) > 1.0f ? absf(exp) : 1.0f;
  return diff <= (TOL * scale);
}

static void smu_start_offsets(uint32_t m_old, uint32_t l_old, uint32_t o_old,
                              uint32_t m_tile, uint32_t l_tile, uint32_t o_tile,
                              uint32_t m_out, uint32_t l_out, uint32_t o_out,
                              uint32_t n, uint32_t d, uint32_t stride) {
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_SRC_M_OLD_REG_OFFSET) = m_old;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_SRC_L_OLD_REG_OFFSET) = l_old;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_SRC_O_OLD_REG_OFFSET) = o_old;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_SRC_M_TILE_REG_OFFSET) = m_tile;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_SRC_L_TILE_REG_OFFSET) = l_tile;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_SRC_O_TILE_REG_OFFSET) = o_tile;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_DST_M_REG_OFFSET) = m_out;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_DST_L_REG_OFFSET) = l_out;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_DST_O_REG_OFFSET) = o_out;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_N_REG_OFFSET) = n;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_D_REG_OFFSET) = d;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_STRIDE_REG_OFFSET) = stride;
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_CTRL_REG_OFFSET) =
      (1u << SPATZ_CLUSTER_PERIPHERAL_MERGE_CTRL_CLEAR_DONE_BIT);
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_CTRL_REG_OFFSET) =
      (1u << SPATZ_CLUSTER_PERIPHERAL_MERGE_CTRL_START_BIT);
}

void smu_start(float *m_old, float *l_old, float *o_old, float *m_tile,
               float *l_tile, float *o_tile, float *m_out, float *l_out,
               float *o_out, uint32_t n, uint32_t d, uint32_t stride) {
  smu_start_offsets(tcdm_off(m_old), tcdm_off(l_old), tcdm_off(o_old),
                    tcdm_off(m_tile), tcdm_off(l_tile), tcdm_off(o_tile),
                    tcdm_off(m_out), tcdm_off(l_out), tcdm_off(o_out), n, d,
                    stride);
}

int smu_done(void) {
  return (*cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_REG_OFFSET) >>
          SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_DONE_BIT) & 1u;
}

static uint32_t smu_status(void) {
  return *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_REG_OFFSET);
}

int smu_error(void) {
  return (smu_status() >> SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_ERROR_BIT) & 1u;
}

static int smu_busy(void) {
  return (smu_status() >> SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_BUSY_BIT) & 1u;
}

static void smu_clear_done(void) {
  *cluster_reg(SPATZ_CLUSTER_PERIPHERAL_MERGE_CTRL_REG_OFFSET) =
      (1u << SPATZ_CLUSTER_PERIPHERAL_MERGE_CTRL_CLEAR_DONE_BIT);
}

static int smu_wait_observe_busy(int *saw_busy) {
  const uint32_t max_polls = 1000000u;
  *saw_busy = 0;
  for (uint32_t i = 0; i < max_polls; i++) {
    uint32_t status = smu_status();
    if ((status >> SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_BUSY_BIT) & 1u) {
      *saw_busy = 1;
    }
    if ((status >> SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_DONE_BIT) & 1u) {
      return 0;
    }
    if ((status >> SPATZ_CLUSTER_PERIPHERAL_MERGE_STATUS_ERROR_BIT) & 1u) {
      return -1;
    }
  }
  PRINTF("SMU timeout, status=0x%x\n", smu_status());
  return -1;
}

int smu_wait(void) {
  int saw_busy;
  return smu_wait_observe_busy(&saw_busy);
}

static void init_case(uint32_t n, uint32_t d, uint32_t case_id) {
  for (uint32_t i = 0; i < n; i++) {
    float base = (float)((int)i - 4) * 0.125f;
    if (case_id == 0) {
      buf->m_old[i] = 1.0f + base;
      buf->m_tile[i] = -0.5f + base;
    } else if (case_id == 1) {
      buf->m_old[i] = -0.75f + base;
      buf->m_tile[i] = 0.5f + base;
    } else {
      buf->m_old[i] = 0.25f + base;
      buf->m_tile[i] = 0.25f + base;
    }
    if (case_id == 0) {
      buf->l_old[i] = 0.75f + 0.0625f * (float)i;
      buf->l_tile[i] = 0.0f;
    } else if (case_id == 1) {
      buf->l_old[i] = 0.0f;
      buf->l_tile[i] = 0.5f + 0.03125f * (float)(i + 1);
    } else if (case_id == 3) {
      buf->l_old[i] = 0.001f * (float)(i + 1);
      buf->l_tile[i] = buf->l_old[i];
    } else if (case_id == 4) {
      buf->l_old[i] = 0.0015f * (float)(i + 1);
      buf->l_tile[i] = buf->l_old[i];
    } else {
      buf->l_old[i] = 0.75f + 0.0625f * (float)i;
      buf->l_tile[i] = buf->l_old[i];
    }
    buf->m_out[i] = 0.0f;
    buf->l_out[i] = 0.0f;
    buf->m_ref[i] = 0.0f;
    buf->l_ref[i] = 0.0f;
    for (uint32_t j = 0; j < d; j++) {
      buf->o_old[i][j] = 0.01f * (float)((int)(i * 7 + j * 3) - 19);
      if (case_id == 0 || case_id == 1) {
        buf->o_tile[i][j] = 0.02f * (float)((int)(i * 5 + j) - 11);
      } else {
        buf->o_tile[i][j] = buf->o_old[i][j];
      }
      buf->o_out[i][j] = 0.0f;
      buf->o_ref[i][j] = 0.0f;
    }
  }
}

static void ref_merge(uint32_t n, uint32_t d, uint32_t case_id) {
  for (uint32_t i = 0; i < n; i++) {
    float old_scale;
    float tile_scale;
    float m_new;
    float l_new;

    if (case_id == 0) {
      m_new = buf->m_old[i];
      old_scale = 1.0f;
      tile_scale = 0.0f;
      l_new = buf->l_old[i];
    } else if (case_id == 1) {
      m_new = buf->m_tile[i];
      old_scale = 0.0f;
      tile_scale = 1.0f;
      l_new = buf->l_tile[i];
    } else {
      m_new = buf->m_old[i];
      old_scale = 0.5f;
      tile_scale = 0.5f;
      l_new = buf->l_old[i] + buf->l_tile[i];
    }
    buf->m_ref[i] = m_new;
    buf->l_ref[i] = l_new;
    for (uint32_t j = 0; j < d; j++) {
      buf->o_ref[i][j] = buf->o_old[i][j] * old_scale + buf->o_tile[i][j] * tile_scale;
    }
  }
}

static int check_case(uint32_t n, uint32_t d) {
  for (uint32_t i = 0; i < n; i++) {
    if (!fp_close(buf->m_out[i], buf->m_ref[i]) ||
        !fp_close(buf->l_out[i], buf->l_ref[i])) {
      PRINTF("Mismatch scalar row %u: m got 0x%x ref 0x%x, l got 0x%x ref 0x%x\n", i,
             *(uint32_t *)&buf->m_out[i], *(uint32_t *)&buf->m_ref[i],
             *(uint32_t *)&buf->l_out[i], *(uint32_t *)&buf->l_ref[i]);
      return -1;
    }
    for (uint32_t j = 0; j < d; j++) {
      if (!fp_close(buf->o_out[i][j], buf->o_ref[i][j])) {
        PRINTF("Mismatch O[%u][%u]: got 0x%x ref 0x%x\n", i, j,
               *(uint32_t *)&buf->o_out[i][j], *(uint32_t *)&buf->o_ref[i][j]);
        return -1;
      }
    }
  }
  return 0;
}

static void init_packed_vectors(uint32_t n, uint32_t d) {
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t j = 0; j < d; j++) {
      uint32_t idx = i * d + j;
      buf->o_old_packed[idx] = buf->o_old[i][j];
      buf->o_tile_packed[idx] = buf->o_tile[i][j];
      buf->o_out_packed[idx] = 0.0f;
    }
  }
}

static int check_packed_case(uint32_t n, uint32_t d) {
  for (uint32_t i = 0; i < n; i++) {
    if (!fp_close(buf->m_out[i], buf->m_ref[i]) ||
        !fp_close(buf->l_out[i], buf->l_ref[i])) {
      PRINTF("Mismatch packed scalar row %u: m got 0x%x ref 0x%x, "
             "l got 0x%x ref 0x%x\n",
             i, *(uint32_t *)&buf->m_out[i], *(uint32_t *)&buf->m_ref[i],
             *(uint32_t *)&buf->l_out[i], *(uint32_t *)&buf->l_ref[i]);
      return -1;
    }
    for (uint32_t j = 0; j < d; j++) {
      uint32_t idx = i * d + j;
      if (!fp_close(buf->o_out_packed[idx], buf->o_ref[i][j])) {
        PRINTF("Mismatch packed O[%u][%u]: got 0x%x ref 0x%x\n", i, j,
               *(uint32_t *)&buf->o_out_packed[idx],
               *(uint32_t *)&buf->o_ref[i][j]);
        return -1;
      }
    }
  }
  return 0;
}

static int smu_wait_error(void) {
  const uint32_t max_polls = 1000000u;
  for (uint32_t i = 0; i < max_polls; i++) {
    if (smu_error()) {
      return smu_busy() ? -1 : 0;
    }
    if (smu_done()) {
      return -1;
    }
  }
  PRINTF("SMU error timeout, status=0x%x\n", smu_status());
  return -1;
}

static int run_invalid_case(const char *name, uint32_t n, uint32_t d,
                            uint32_t m_old_offset, uint32_t stride) {
  smu_start_offsets(m_old_offset, tcdm_off(buf->l_old),
                    tcdm_off(&buf->o_old[0][0]), tcdm_off(buf->m_tile),
                    tcdm_off(buf->l_tile), tcdm_off(&buf->o_tile[0][0]),
                    tcdm_off(buf->m_out), tcdm_off(buf->l_out),
                    tcdm_off(&buf->o_out[0][0]), n, d, stride);
  if (smu_wait_error() != 0) {
    PRINTF("SMU invalid-config case %s did not report clean error, status=0x%x\n",
           name, smu_status());
    return -1;
  }
  PRINTF("online-softmax-merge invalid %s status=0x%x\n", name, smu_status());
  smu_clear_done();
  return 0;
}

static int run_invalid_cases(void) {
  uint32_t m_old = tcdm_off(buf->m_old);
  int rc = 0;
  rc |= run_invalid_case("n-zero", 0, 1, m_old, MAX_D * sizeof(float));
  rc |= run_invalid_case("d-zero", 1, 0, m_old, MAX_D * sizeof(float));
  rc |= run_invalid_case("misaligned-address", 1, 1, m_old + 1,
                         MAX_D * sizeof(float));
  rc |= run_invalid_case("misaligned-stride", 1, 1, m_old, 2);
  return rc;
}

static int run_unsupported_case(const char *name, float m_old, float m_tile,
                                float l_old, float l_tile) {
  init_case(1, 1, 2);
  buf->m_old[0] = m_old;
  buf->m_tile[0] = m_tile;
  buf->l_old[0] = l_old;
  buf->l_tile[0] = l_tile;
  smu_start(buf->m_old, buf->l_old, &buf->o_old[0][0], buf->m_tile, buf->l_tile,
            &buf->o_tile[0][0], buf->m_out, buf->l_out, &buf->o_out[0][0], 1, 1,
            MAX_D * sizeof(float));
  if (smu_wait_error() != 0) {
    PRINTF("SMU unsupported case %s did not report error, status=0x%x\n", name,
           smu_status());
    return -1;
  }
  PRINTF("online-softmax-merge unsupported %s status=0x%x\n", name, smu_status());
  smu_clear_done();
  return 0;
}

static int run_unsupported_cases(void) {
  int rc = 0;
  rc |= run_unsupported_case("mixed-m", 0.0f, 1.0f, 1.0f, 1.0f);
  rc |= run_unsupported_case("unequal-l", 1.0f, 1.0f, 1.0f, 2.0f);
  return rc;
}

static int run_case(uint32_t n, uint32_t d, uint32_t case_id, int require_busy) {
  init_case(n, d, case_id);
  uint32_t cpu_start = benchmark_get_cycle();
  ref_merge(n, d, case_id);
  uint32_t cpu_cycles = benchmark_get_cycle() - cpu_start;

  snrt_reset_perf_counter(SNRT_PERF_CNT0);
  snrt_reset_perf_counter(SNRT_PERF_CNT1);
  snrt_start_perf_counter(SNRT_PERF_CNT0, SNRT_PERF_CNT_TCDM_ACCESSED, 0);
  snrt_start_perf_counter(SNRT_PERF_CNT1, SNRT_PERF_CNT_TCDM_CONGESTED, 0);
  uint32_t engine_start = benchmark_get_cycle();
  smu_start(buf->m_old, buf->l_old, &buf->o_old[0][0], buf->m_tile, buf->l_tile,
            &buf->o_tile[0][0], buf->m_out, buf->l_out, &buf->o_out[0][0], n, d,
            MAX_D * sizeof(float));
  int saw_busy;
  int wait_rc = smu_wait_observe_busy(&saw_busy);
  uint32_t engine_cycles = benchmark_get_cycle() - engine_start;
  snrt_stop_perf_counter(SNRT_PERF_CNT0);
  snrt_stop_perf_counter(SNRT_PERF_CNT1);
  uint32_t tcdm_accessed = snrt_get_perf_counter(SNRT_PERF_CNT0);
  uint32_t tcdm_congested = snrt_get_perf_counter(SNRT_PERF_CNT1);

  if (wait_rc != 0 || smu_error()) {
    PRINTF("SMU error for N=%u D=%u case=%u\n", n, d, case_id);
    return -1;
  }
  if (require_busy && !saw_busy) {
    PRINTF("SMU busy was not observed for N=%u D=%u case=%u\n", n, d,
           case_id);
    return -1;
  }
  if (check_case(n, d) != 0) {
    return -1;
  }

  PRINTF("online-softmax-merge N=%u D=%u case=%u cpu=%u engine=%u "
         "tcdm_accessed=%u tcdm_congested=%u\n",
         n, d, case_id, cpu_cycles, engine_cycles, tcdm_accessed,
         tcdm_congested);
  return 0;
}

static int run_stride_zero_case(uint32_t n, uint32_t d, uint32_t case_id) {
  init_case(n, d, case_id);
  init_packed_vectors(n, d);
  ref_merge(n, d, case_id);

  smu_start(buf->m_old, buf->l_old, buf->o_old_packed, buf->m_tile,
            buf->l_tile, buf->o_tile_packed, buf->m_out, buf->l_out,
            buf->o_out_packed, n, d, 0);
  int saw_busy;
  if (smu_wait_observe_busy(&saw_busy) != 0 || smu_error()) {
    PRINTF("SMU error for stride-zero N=%u D=%u case=%u status=0x%x\n", n, d,
           case_id, smu_status());
    return -1;
  }
  if (!saw_busy) {
    PRINTF("SMU busy was not observed for stride-zero N=%u D=%u case=%u\n",
           n, d, case_id);
    return -1;
  }
  if (check_packed_case(n, d) != 0) {
    return -1;
  }

  PRINTF("online-softmax-merge stride-zero N=%u D=%u case=%u status=0x%x\n", n,
         d, case_id, smu_status());
  smu_clear_done();
  return 0;
}

int main(void) {
  if (snrt_cluster_core_idx() != 0) {
    snrt_cluster_hw_barrier();
    return 0;
  }

  buf = (smu_buffers_t *)snrt_l1alloc(sizeof(smu_buffers_t));
  if (buf == 0) {
    PRINTF("Failed to allocate SMU buffers\n");
    return -1;
  }

  int rc = 0;
  rc |= run_case(1, 1, 0, 0);
  rc |= run_case(4, 8, 1, 0);
  rc |= run_case(16, 64, 2, 1);
  rc |= run_case(4, 8, 3, 0);
  rc |= run_case(4, 8, 4, 0);
  rc |= run_stride_zero_case(4, 8, 2);
  rc |= run_invalid_cases();
  rc |= run_unsupported_cases();

  if (rc == 0) {
    PRINTF("online-softmax-merge PASS\n");
  }

  snrt_cluster_hw_barrier();
  return rc;
}
