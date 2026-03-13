// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

typedef enum
{
    FP64 = 8,
    FP32 = 4,
    FP16 = 2,
    FP8 = 1
} precision_t;

typedef struct
{
    double real;
    double imag;
} cdouble;

typedef struct
{
    float real;
    float imag;
} cfloat;

typedef struct
{
    _Float16 real;
    _Float16 imag;
} chalf;

typedef struct gemm_layer_struct gemm_layer;
typedef struct conv_layer_struct conv_layer;
typedef struct dotp_layer_struct
{
    uint32_t M;
    precision_t dtype;
} dotp_layer;
