// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

namespace {

float quantize(float x, int frac_bits, int acc_bits) {
  if (frac_bits < 0) {
    return x;
  }

  const float scale = std::ldexp(1.0f, frac_bits);
  float q = std::round(x * scale) / scale;

  if (acc_bits > frac_bits + 1) {
    const int int_bits = acc_bits - frac_bits - 1;
    const float max_value = std::ldexp(1.0f, int_bits) - 1.0f / scale;
    const float min_value = -std::ldexp(1.0f, int_bits);
    if (q > max_value) {
      q = max_value;
    }
    if (q < min_value) {
      q = min_value;
    }
  }
  return q;
}

float exp_lut(float x, int entries, int frac_bits, int acc_bits) {
  constexpr float log2_e = 1.442695041f;
  float y = quantize(x * log2_e, frac_bits, acc_bits);
  float ipart_f;
  float frac = std::modf(y, &ipart_f);
  int ipart = static_cast<int>(ipart_f);
  if (frac < 0.0f) {
    frac += 1.0f;
    ipart--;
  }

  int idx = static_cast<int>(frac * entries);
  if (idx < 0) {
    idx = 0;
  }
  if (idx >= entries) {
    idx = entries - 1;
  }
  const float x0 = static_cast<float>(idx) / static_cast<float>(entries);
  const float x1 = static_cast<float>(idx + 1) / static_cast<float>(entries);
  const float y0 = std::exp2(x0);
  const float y1 = std::exp2(x1);
  const float t = (frac - x0) / (x1 - x0);
  float mant = quantize(y0 + (y1 - y0) * t, frac_bits, acc_bits);
  return quantize(std::ldexp(mant, ipart), frac_bits, acc_bits);
}

struct Summary {
  float m = -std::numeric_limits<float>::max();
  float l = 0.0f;
  std::vector<float> o;
};

void merge_ref(Summary& acc, const Summary& tile) {
  if (tile.l == 0.0f) {
    return;
  }
  if (acc.l == 0.0f) {
    acc = tile;
    return;
  }

  const float m_new = std::max(acc.m, tile.m);
  const float old_scale = std::exp(acc.m - m_new);
  const float tile_scale = std::exp(tile.m - m_new);
  const float l_new = acc.l * old_scale + tile.l * tile_scale;
  for (std::size_t i = 0; i < acc.o.size(); i++) {
    acc.o[i] =
        (acc.o[i] * acc.l * old_scale + tile.o[i] * tile.l * tile_scale) /
        l_new;
  }
  acc.m = m_new;
  acc.l = l_new;
}

void merge_lut(Summary& acc, const Summary& tile, int entries, int frac_bits,
               int acc_bits) {
  if (tile.l == 0.0f) {
    return;
  }
  if (acc.l == 0.0f) {
    acc = tile;
    for (float& x : acc.o) {
      x = quantize(x, frac_bits, acc_bits);
    }
    acc.l = quantize(acc.l, frac_bits, acc_bits);
    return;
  }

  const float m_new = std::max(acc.m, tile.m);
  const float old_scale = exp_lut(acc.m - m_new, entries, frac_bits, acc_bits);
  const float tile_scale = exp_lut(tile.m - m_new, entries, frac_bits, acc_bits);
  const float l_new =
      quantize(acc.l * old_scale + tile.l * tile_scale, frac_bits, acc_bits);
  for (std::size_t i = 0; i < acc.o.size(); i++) {
    acc.o[i] = quantize((acc.o[i] * acc.l * old_scale +
                         tile.o[i] * tile.l * tile_scale) /
                            l_new,
                        frac_bits, acc_bits);
  }
  acc.m = m_new;
  acc.l = l_new;
}

Summary make_tile(int tile_id, int dim) {
  Summary tile;
  tile.o.resize(dim);
  tile.m = -0.35f + 0.19f * static_cast<float>((tile_id * 5) % 9);
  tile.l = 0.5f + 0.07f * static_cast<float>((tile_id * 7) % 11);
  for (int d = 0; d < dim; d++) {
    const int v = (tile_id * 13 + d * 5) % 31;
    tile.o[d] = (static_cast<float>(v) - 15.0f) * 0.03125f;
  }
  return tile;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: " << argv[0]
              << " <lut_entries> <fraction_bits> <accumulator_bits>\n";
    return 2;
  }

  const int entries = std::atoi(argv[1]);
  const int frac_bits = std::atoi(argv[2]);
  const int acc_bits = std::atoi(argv[3]);
  constexpr int dim = 32;
  constexpr int tiles = 6;

  Summary ref;
  Summary got;
  ref.o.assign(dim, 0.0f);
  got.o.assign(dim, 0.0f);

  for (int t = 0; t < tiles; t++) {
    const Summary tile = make_tile(t, dim);
    merge_ref(ref, tile);
    merge_lut(got, tile, entries, frac_bits, acc_bits);
  }

  float max_abs = std::abs(got.l - ref.l);
  float max_rel = max_abs / std::max(std::abs(ref.l), 1.0f);
  for (int d = 0; d < dim; d++) {
    const float diff = std::abs(got.o[d] - ref.o[d]);
    const float rel = diff / std::max(std::abs(ref.o[d]), 1.0f);
    max_abs = std::max(max_abs, diff);
    max_rel = std::max(max_rel, rel);
  }

  std::cout << std::scientific << std::setprecision(6) << "max_abs="
            << max_abs << " max_rel=" << max_rel << "\n";
  return 0;
}
