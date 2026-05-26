// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>

#ifdef ATTNRES_HOST_TEST
#include <stdlib.h>
#else
#include <benchmark.h>
#include <debug.h>
#include <perf_cnt.h>
#include <snrt.h>
#endif

#undef PRINTF
#define PRINTF(...) printf(__VA_ARGS__)

#define MAX_L 4
#define MAX_H 1
#define MAX_Q 8
#define MAX_D 32
#define MAX_HIST_BLOCKS 4
#define MAX_CUR_BLOCKS 1
#define MAX_BLOCKS (MAX_HIST_BLOCKS + MAX_CUR_BLOCKS)
#define MAX_TOKENS (MAX_BLOCKS * MAX_Q)
#define LUT_ENTRIES 32
#define LOG2_E 1.442695041f
#define NEG_INF (-3.402823466e+38f)
#define ERR_TOL 1.0e-3f

typedef struct {
  float value[MAX_L][MAX_H][MAX_BLOCKS][MAX_Q][MAX_D];
  float ref[MAX_L][MAX_H][MAX_Q][MAX_D];
  float out[MAX_L][MAX_H][MAX_Q][MAX_D];
  float hist_m[MAX_L][MAX_H][MAX_Q];
  float hist_l[MAX_L][MAX_H][MAX_Q];
  float hist_o[MAX_L][MAX_H][MAX_Q][MAX_D];
  float work_scores[MAX_TOKENS];
  float work_exp[MAX_TOKENS];
} attnres_buffers_t;

typedef struct {
  uint32_t layers;
  uint32_t heads;
  uint32_t q_rows;
  uint32_t dim;
  uint32_t hist_blocks;
  uint32_t cur_blocks;
} attnres_cfg_t;

typedef struct {
  uint32_t hist_bytes;
  uint32_t partial_bytes;
  uint32_t state_bytes;
} traffic_t;

typedef struct {
  uint32_t cycles;
  uint32_t tcdm_accessed;
  uint32_t tcdm_congested;
  traffic_t traffic;
} run_metrics_t;

static attnres_buffers_t *buf;
static volatile uint32_t alloc_failed;

#ifdef ATTNRES_HOST_TEST
static uint32_t host_cycle;

static uint32_t benchmark_get_cycle(void) { return host_cycle += 100; }
static void snrt_reset_perf_counter(uint32_t perf_cnt) { (void)perf_cnt; }
static void snrt_start_perf_counter(uint32_t perf_cnt, uint32_t type,
                                    uint32_t hart_id) {
  (void)perf_cnt;
  (void)type;
  (void)hart_id;
}
static void snrt_stop_perf_counter(uint32_t perf_cnt) { (void)perf_cnt; }
static uint32_t snrt_get_perf_counter(uint32_t perf_cnt) {
  (void)perf_cnt;
  return 0;
}
#define SNRT_PERF_CNT0 0
#define SNRT_PERF_CNT1 1
#define SNRT_PERF_CNT_TCDM_ACCESSED 0
#define SNRT_PERF_CNT_TCDM_CONGESTED 1
#endif

static const float exp2_lut[LUT_ENTRIES + 1] = {
    1.000000000f, 1.021897149f, 1.044273782f, 1.067140401f, 1.090507733f,
    1.114386743f, 1.138788635f, 1.163724859f, 1.189207115f, 1.215247360f,
    1.241857812f, 1.269050957f, 1.296839555f, 1.325236643f, 1.354255547f,
    1.383909882f, 1.414213562f, 1.445180807f, 1.476826146f, 1.509164428f,
    1.542210825f, 1.575980845f, 1.610490332f, 1.645755478f, 1.681792831f,
    1.718619298f, 1.756252160f, 1.794709075f, 1.834008086f, 1.874167634f,
    1.915206561f, 1.957144124f, 2.000000000f};

static float absf_local(float x) { return x < 0.0f ? -x : x; }

static float exp2_scale(float x, int shift) {
  if (x == 0.0f) {
    return 0.0f;
  }
  while (shift > 0) {
    x *= 2.0f;
    shift--;
  }
  while (shift < 0) {
    x *= 0.5f;
    shift++;
  }
  return x;
}

static float attn_exp(float x) {
  float y = x * LOG2_E;
  int ipart = (int)y;
  if (y < 0.0f && (float)ipart != y) {
    ipart--;
  }

  float frac = y - (float)ipart;
  if (frac < 0.0f) {
    frac += 1.0f;
    ipart--;
  }

  int idx = (int)(frac * (float)LUT_ENTRIES);
  if (idx < 0) {
    idx = 0;
  }
  if (idx >= LUT_ENTRIES) {
    idx = LUT_ENTRIES - 1;
  }
  float t = frac * (float)LUT_ENTRIES - (float)idx;
  float mant = exp2_lut[idx] + (exp2_lut[idx + 1] - exp2_lut[idx]) * t;
  return exp2_scale(mant, ipart);
}

static void traffic_add(traffic_t *traffic, uint32_t *field, uint32_t count) {
  uint32_t bytes = count * sizeof(float);
  *field += bytes;
  (void)traffic;
}

static float synthetic_score(uint32_t layer, uint32_t head, uint32_t q,
                             uint32_t block, uint32_t token, uint32_t dim) {
  int v = (int)(layer * 29 + head * 17 + q * 11 + block * 7 + token * 5 +
                dim * 3) %
          43;
  return ((float)v - 21.0f) * 0.03125f;
}

static void zero_vec(float *vec, uint32_t dim) {
  for (uint32_t d = 0; d < dim; d++) {
    vec[d] = 0.0f;
  }
}

static void copy_vec(float *dst, const float *src, uint32_t dim) {
  for (uint32_t d = 0; d < dim; d++) {
    dst[d] = src[d];
  }
}

static void merge_summary(float *m_acc, float *l_acc, float *o_acc,
                          float m_tile, float l_tile, const float *o_tile,
                          uint32_t dim) {
  if (l_tile == 0.0f) {
    return;
  }
  if (*l_acc == 0.0f) {
    *m_acc = m_tile;
    *l_acc = l_tile;
    copy_vec(o_acc, o_tile, dim);
    return;
  }

  float m_new = *m_acc > m_tile ? *m_acc : m_tile;
  float old_scale = attn_exp(*m_acc - m_new);
  float tile_scale = attn_exp(m_tile - m_new);
  float l_new = *l_acc * old_scale + l_tile * tile_scale;
  float inv_l = 1.0f / l_new;

  for (uint32_t d = 0; d < dim; d++) {
    o_acc[d] = (o_acc[d] * *l_acc * old_scale +
                o_tile[d] * l_tile * tile_scale) *
               inv_l;
  }
  *m_acc = m_new;
  *l_acc = l_new;
}

static void compute_block_summary(const attnres_cfg_t *cfg, uint32_t layer,
                                  uint32_t head, uint32_t q, uint32_t block,
                                  uint32_t count_hist,
                                  uint32_t count_partial, float *m_out,
                                  float *l_out, float *o_out) {
  float scores[MAX_Q];
  float max_score = NEG_INF;
  zero_vec(o_out, cfg->dim);

  for (uint32_t t = 0; t < cfg->q_rows; t++) {
    float score = synthetic_score(layer, head, q, block, t, cfg->dim);
    scores[t] = score;
    if (score > max_score) {
      max_score = score;
    }
  }

  float l_sum = 0.0f;
  for (uint32_t t = 0; t < cfg->q_rows; t++) {
    float p = attn_exp(scores[t] - max_score);
    l_sum += p;
    for (uint32_t d = 0; d < cfg->dim; d++) {
      o_out[d] += p * buf->value[layer][head][block][t][d];
    }
  }

  if (l_sum != 0.0f) {
    float inv_l = 1.0f / l_sum;
    for (uint32_t d = 0; d < cfg->dim; d++) {
      o_out[d] *= inv_l;
    }
  }

  *m_out = max_score;
  *l_out = l_sum;
  (void)count_hist;
  (void)count_partial;
}

static void run_naive_full_recompute(const attnres_cfg_t *cfg,
                                     traffic_t *traffic) {
  uint32_t total_blocks = cfg->hist_blocks + cfg->cur_blocks;
  uint32_t total_tokens = total_blocks * cfg->q_rows;

  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t q = 0; q < cfg->q_rows; q++) {
        float max_score = NEG_INF;
        for (uint32_t b = 0; b < total_blocks; b++) {
          for (uint32_t t = 0; t < cfg->q_rows; t++) {
            float score = synthetic_score(l, h, q, b, t, cfg->dim);
            buf->work_scores[b * cfg->q_rows + t] = score;
            if (score > max_score) {
              max_score = score;
            }
            if (b < cfg->hist_blocks) {
              traffic_add(traffic, &traffic->hist_bytes, 2 * cfg->dim);
            } else {
              traffic_add(traffic, &traffic->partial_bytes, 2 * cfg->dim);
            }
          }
        }

        float l_sum = 0.0f;
        for (uint32_t i = 0; i < total_tokens; i++) {
          float p = attn_exp(buf->work_scores[i] - max_score);
          buf->work_exp[i] = p;
          l_sum += p;
          traffic_add(traffic, &traffic->state_bytes, 2);
        }

        zero_vec(buf->out[l][h][q], cfg->dim);
        for (uint32_t b = 0; b < total_blocks; b++) {
          for (uint32_t t = 0; t < cfg->q_rows; t++) {
            float p = buf->work_exp[b * cfg->q_rows + t];
            for (uint32_t d = 0; d < cfg->dim; d++) {
              buf->out[l][h][q][d] += p * buf->value[l][h][b][t][d];
            }
            if (b < cfg->hist_blocks) {
              traffic_add(traffic, &traffic->hist_bytes, cfg->dim);
            } else {
              traffic_add(traffic, &traffic->partial_bytes, cfg->dim);
            }
          }
        }

        float inv_l = 1.0f / l_sum;
        for (uint32_t d = 0; d < cfg->dim; d++) {
          buf->out[l][h][q][d] *= inv_l;
        }
        traffic_add(traffic, &traffic->state_bytes, cfg->dim);
      }
    }
  }
}

static void run_paper_two_phase(const attnres_cfg_t *cfg, traffic_t *traffic) {
  float tile_o[MAX_D];

  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t q = 0; q < cfg->q_rows; q++) {
        float m_acc = NEG_INF;
        float l_acc = 0.0f;
        zero_vec(buf->hist_o[l][h][q], cfg->dim);

        for (uint32_t b = 0; b < cfg->hist_blocks; b++) {
          float m_tile;
          float l_tile;
          compute_block_summary(cfg, l, h, q, b, cfg->q_rows, 0, &m_tile,
                                &l_tile, tile_o);
          merge_summary(&m_acc, &l_acc, buf->hist_o[l][h][q], m_tile, l_tile,
                        tile_o, cfg->dim);
          traffic_add(traffic, &traffic->hist_bytes, 2 * cfg->q_rows * cfg->dim);
          traffic_add(traffic, &traffic->state_bytes, 3 + cfg->dim);
        }

        buf->hist_m[l][h][q] = m_acc;
        buf->hist_l[l][h][q] = l_acc;
        traffic_add(traffic, &traffic->state_bytes, 2 + cfg->dim);
      }
    }
  }

  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t q = 0; q < cfg->q_rows; q++) {
        float m_acc = buf->hist_m[l][h][q];
        float l_acc = buf->hist_l[l][h][q];
        copy_vec(buf->out[l][h][q], buf->hist_o[l][h][q], cfg->dim);
        traffic_add(traffic, &traffic->state_bytes, 2 + cfg->dim);

        for (uint32_t b = cfg->hist_blocks; b < cfg->hist_blocks + cfg->cur_blocks;
             b++) {
          float m_tile;
          float l_tile;
          compute_block_summary(cfg, l, h, q, b, 0, cfg->q_rows, &m_tile,
                                &l_tile, tile_o);
          merge_summary(&m_acc, &l_acc, buf->out[l][h][q], m_tile, l_tile,
                        tile_o, cfg->dim);
          traffic_add(traffic, &traffic->partial_bytes,
                      2 * cfg->q_rows * cfg->dim);
          traffic_add(traffic, &traffic->state_bytes, 3 + cfg->dim);
        }
      }
    }
  }
}

static void run_software_fusion(const attnres_cfg_t *cfg, traffic_t *traffic) {
  float tile_o[MAX_D];

  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t q = 0; q < cfg->q_rows; q++) {
        float m_acc = NEG_INF;
        float l_acc = 0.0f;
        zero_vec(buf->out[l][h][q], cfg->dim);

        for (uint32_t b = 0; b < cfg->hist_blocks + cfg->cur_blocks; b++) {
          float m_tile;
          float l_tile;
          compute_block_summary(cfg, l, h, q, b, cfg->q_rows, cfg->q_rows,
                                &m_tile, &l_tile, tile_o);
          merge_summary(&m_acc, &l_acc, buf->out[l][h][q], m_tile, l_tile,
                        tile_o, cfg->dim);
          if (b < cfg->hist_blocks) {
            traffic_add(traffic, &traffic->hist_bytes,
                        2 * cfg->q_rows * cfg->dim);
          } else {
            traffic_add(traffic, &traffic->partial_bytes,
                        2 * cfg->q_rows * cfg->dim);
          }
          traffic_add(traffic, &traffic->state_bytes, 2);
        }
        traffic_add(traffic, &traffic->state_bytes, cfg->dim);
      }
    }
  }
}

static void fill_inputs(const attnres_cfg_t *cfg) {
  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t b = 0; b < cfg->hist_blocks + cfg->cur_blocks; b++) {
        for (uint32_t t = 0; t < cfg->q_rows; t++) {
          for (uint32_t d = 0; d < cfg->dim; d++) {
            int vv = (int)(l * 23 + h * 3 + b * 7 + t * 11 + d * 5) % 41;
            buf->value[l][h][b][t][d] = ((float)vv - 20.0f) * 0.015625f;
          }
        }
      }
    }
  }
}

static void clear_outputs(const attnres_cfg_t *cfg) {
  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t q = 0; q < cfg->q_rows; q++) {
        zero_vec(buf->out[l][h][q], cfg->dim);
      }
    }
  }
}

static void copy_ref(const attnres_cfg_t *cfg) {
  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t q = 0; q < cfg->q_rows; q++) {
        copy_vec(buf->ref[l][h][q], buf->out[l][h][q], cfg->dim);
      }
    }
  }
}

static void calc_error(const attnres_cfg_t *cfg, float *max_abs,
                       float *max_rel) {
  *max_abs = 0.0f;
  *max_rel = 0.0f;
  for (uint32_t l = 0; l < cfg->layers; l++) {
    for (uint32_t h = 0; h < cfg->heads; h++) {
      for (uint32_t q = 0; q < cfg->q_rows; q++) {
        for (uint32_t d = 0; d < cfg->dim; d++) {
          float ref = buf->ref[l][h][q][d];
          float diff = absf_local(buf->out[l][h][q][d] - ref);
          float scale = absf_local(ref) > 1.0f ? absf_local(ref) : 1.0f;
          float rel = diff / scale;
          if (diff > *max_abs) {
            *max_abs = diff;
          }
          if (rel > *max_rel) {
            *max_rel = rel;
          }
        }
      }
    }
  }
}

static void measure_baseline(const attnres_cfg_t *cfg, const char *name,
                             void (*fn)(const attnres_cfg_t *, traffic_t *),
                             run_metrics_t *metrics) {
  metrics->traffic.hist_bytes = 0;
  metrics->traffic.partial_bytes = 0;
  metrics->traffic.state_bytes = 0;
  clear_outputs(cfg);

  snrt_reset_perf_counter(SNRT_PERF_CNT0);
  snrt_reset_perf_counter(SNRT_PERF_CNT1);
  snrt_start_perf_counter(SNRT_PERF_CNT0, SNRT_PERF_CNT_TCDM_ACCESSED, 0);
  snrt_start_perf_counter(SNRT_PERF_CNT1, SNRT_PERF_CNT_TCDM_CONGESTED, 0);
  uint32_t start = benchmark_get_cycle();
  fn(cfg, &metrics->traffic);
  metrics->cycles = benchmark_get_cycle() - start;
  snrt_stop_perf_counter(SNRT_PERF_CNT0);
  snrt_stop_perf_counter(SNRT_PERF_CNT1);
  metrics->tcdm_accessed = snrt_get_perf_counter(SNRT_PERF_CNT0);
  metrics->tcdm_congested = snrt_get_perf_counter(SNRT_PERF_CNT1);
  (void)name;
}

static void print_result(const attnres_cfg_t *cfg, const char *name,
                         const run_metrics_t *metrics, float max_abs,
                         float max_rel) {
  PRINTF("attnres-baseline name=%s L=%u Q=%u D=%u hist_blocks=%u "
         "cycles=%u tcdm_accessed=%u tcdm_congested=%u hist_bytes=%u "
         "partial_bytes=%u state_bytes=%u max_abs_err=%e max_rel_err=%e\n",
         name, cfg->layers, cfg->q_rows, cfg->dim, cfg->hist_blocks,
         metrics->cycles, metrics->tcdm_accessed, metrics->tcdm_congested,
         metrics->traffic.hist_bytes, metrics->traffic.partial_bytes,
         metrics->traffic.state_bytes, max_abs, max_rel);
}

static int run_config(const attnres_cfg_t *cfg) {
  run_metrics_t metrics;
  float max_abs;
  float max_rel;

  fill_inputs(cfg);

  measure_baseline(cfg, "naive-full-recompute", run_naive_full_recompute,
                   &metrics);
  copy_ref(cfg);
  print_result(cfg, "naive-full-recompute", &metrics, 0.0f, 0.0f);

  measure_baseline(cfg, "paper-two-phase", run_paper_two_phase, &metrics);
  calc_error(cfg, &max_abs, &max_rel);
  print_result(cfg, "paper-two-phase", &metrics, max_abs, max_rel);
  if (max_abs > ERR_TOL || max_rel > ERR_TOL) {
    PRINTF("attnres-baselines mismatch name=paper-two-phase max_abs_err=%e "
           "max_rel_err=%e\n",
           max_abs, max_rel);
    return -1;
  }

  measure_baseline(cfg, "software-fusion", run_software_fusion, &metrics);
  calc_error(cfg, &max_abs, &max_rel);
  print_result(cfg, "software-fusion", &metrics, max_abs, max_rel);
  if (max_abs > ERR_TOL || max_rel > ERR_TOL) {
    PRINTF("attnres-baselines mismatch name=software-fusion max_abs_err=%e "
           "max_rel_err=%e\n",
           max_abs, max_rel);
    return -1;
  }

  return 0;
}

int main() {
#ifdef ATTNRES_HOST_TEST
  buf = (attnres_buffers_t *)calloc(1, sizeof(attnres_buffers_t));
  if (buf == 0) {
    PRINTF("attnres-baselines allocation failed\n");
    return -1;
  }
#else
  uint32_t cid = snrt_cluster_core_idx();

  if (cid == 0) {
    buf = (attnres_buffers_t *)snrt_l1alloc(sizeof(attnres_buffers_t));
    if (buf == 0) {
      alloc_failed = 1;
    }
  }

  snrt_cluster_hw_barrier();
  if (alloc_failed) {
    if (cid == 0) {
      PRINTF("attnres-baselines allocation failed\n");
    }
    snrt_cluster_hw_barrier();
    return cid == 0 ? -1 : 0;
  }

  if (cid != 0) {
    snrt_cluster_hw_barrier();
    return 0;
  }
#endif

#if defined(ATTNRES_HOST_TEST) || defined(ATTNRES_FULL_SWEEP)
  const uint32_t layers[] = {2, 4};
  const uint32_t dims[] = {16, 32};
  const uint32_t hist_blocks[] = {2, 4};
#else
  const uint32_t layers[] = {2};
  const uint32_t dims[] = {16};
  const uint32_t hist_blocks[] = {2};
#endif

  for (uint32_t li = 0; li < sizeof(layers) / sizeof(layers[0]); li++) {
    for (uint32_t di = 0; di < sizeof(dims) / sizeof(dims[0]); di++) {
      for (uint32_t bi = 0; bi < sizeof(hist_blocks) / sizeof(hist_blocks[0]);
           bi++) {
        attnres_cfg_t cfg = {.layers = layers[li],
                             .heads = 1,
                             .q_rows = 8,
                             .dim = dims[di],
                             .hist_blocks = hist_blocks[bi],
                             .cur_blocks = 1};
        if (run_config(&cfg) != 0) {
          return -1;
        }
      }
    }
  }

  PRINTF("attnres-baselines verification=SUCCESS\n");
#ifndef ATTNRES_HOST_TEST
  snrt_cluster_hw_barrier();
#endif
  return 0;
}
