// Copyright 2020 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
typedef enum { FP64 = 8, FP32 = 4, FP16 = 2, FP8 = 1 } precision_t;
typedef struct { double real; double imag; } cdouble;
typedef struct { float real; float imag; } cfloat;
typedef struct { _Float16 real; _Float16 imag; } chalf;
