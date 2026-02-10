#!/usr/bin/env python3
# Modified for Complex Kernels

import numpy as np
import torch
import torch.nn as nn
import argparse
import pathlib
import hjson

# Fix random seeds for reproducibility
np.random.seed(42)
torch.manual_seed(42)

def array_to_cstr(a, fmt=float):
    """Converts a flat array to a C-style initializer string."""
    out = "{"
    if isinstance(a, torch.Tensor):
        a = a.detach().cpu().numpy().flat
    elif isinstance(a, np.ndarray):
        a = a.flat
        
    for el in a:
        if fmt == float:
            out += "{:.8f}, ".format(el)
        else:
            out += "{}, ".format(el)
            
    out = out[:-2] + "}"
    return out

def complex_tensor_to_cstr(t):
    """
    Flattens a complex tensor into interleaved Real/Imag format for C arrays.
    Input: Complex Tensor shape (M, N)
    Output: String "{Re, Im, Re, Im...}"
    """
    # 1. View as real -> (M, N, 2)
    # 2. Flatten -> (M*N*2) -> [Re0, Im0, Re1, Im1...]
    if t.is_complex():
        flat = torch.view_as_real(t).flatten()
    else:
        # Fallback if already real/imag separated
        flat = t.flatten()
    return array_to_cstr(flat, fmt=float)

def get_c_type_name(prec):
    """Maps precision to C struct and scalar types."""
    if prec == 64: return "cdouble", "double"
    if prec == 32: return "cfloat", "float"
    if prec == 16: return "chalf", "__fp16"
    raise ValueError("Unsupported precision")

def rand_complex(shape, dtype):
    """Generates random complex data."""
    # Use standard normal distribution
    real = torch.randn(shape, dtype=dtype)
    imag = torch.randn(shape, dtype=dtype)
    return torch.complex(real, imag)

# ==========================================
# Emitters for specific Kernels
# ==========================================

def emit_caxpy(name, **kwargs):
    M = kwargs["M"]
    prec = kwargs["prec"]
    X = kwargs["X"]
    Y = kwargs["Y"]
    alpha = kwargs["alpha"]
    result = kwargs["result"]

    c_struct, c_scalar = get_c_type_name(prec)
    
    s = '#include "layer.h"\n\n'
    s += f"// Kernel: CAXPY (y = alpha * x + y), Precision: {prec}-bit\n"
    s += f"const int {name}_n = {M};\n\n"

    # Alpha is a scalar complex
    s += f"static {c_struct} {name}_alpha = {{{alpha.real.item()}, {alpha.imag.item()}}};\n\n"
    
    # Vectors
    s += f"static {c_struct} {name}_x[{M}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(X) + ";\n"
    s += f"static {c_struct} {name}_y[{M}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(Y) + ";\n"
    s += f"static {c_struct} {name}_result[{M}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(result) + ";\n"
    
    return s

def emit_cdotp(name, **kwargs):
    M = kwargs["M"]
    prec = kwargs["prec"]
    X = kwargs["X"]
    Y = kwargs["Y"]
    result = kwargs["result"] # Scalar Complex

    c_struct, c_scalar = get_c_type_name(prec)
    
    s = '#include "layer.h"\n\n'
    s += f"// Kernel: CDOTP (Dot Product), Precision: {prec}-bit\n"
    s += f"const int {name}_n = {M};\n\n"

    s += f"static {c_struct} {name}_x[{M}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(X) + ";\n"
    s += f"static {c_struct} {name}_y[{M}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(Y) + ";\n"
    
    # Result is a scalar structure
    s += f"static {c_struct} {name}_result = {{{result.real.item()}, {result.imag.item()}}};\n"
    
    return s

def emit_cmatmul(name, **kwargs):
    M, N, K = kwargs["M"], kwargs["N"], kwargs["K"]
    prec = kwargs["prec"]
    A = kwargs["A"]
    B = kwargs["B"]
    C = kwargs["C"] # Initial C
    result = kwargs["result"] # Final C

    c_struct, c_scalar = get_c_type_name(prec)
    
    s = '#include "layer.h"\n\n'
    s += f"// Kernel: CMATMUL (C += A * B), Precision: {prec}-bit\n"
    s += f"const int {name}_M = {M};\n"
    s += f"const int {name}_N = {N};\n"
    s += f"const int {name}_P = {N}; // Assuming Row Major, P=N\n\n"

    s += f"static {c_struct} {name}_a[{M}*{K}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(A) + ";\n"
    s += f"static {c_struct} {name}_b[{K}*{N}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(B) + ";\n"
    s += f"static {c_struct} {name}_c[{M}*{N}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(C) + ";\n"
    s += f"static {c_struct} {name}_result[{M}*{N}] __attribute__((section(\".data\"))) = " + complex_tensor_to_cstr(result) + ";\n"
    
    return s

# ==========================================
# Main Dispatcher
# ==========================================

def main():
    parser = argparse.ArgumentParser(description="Generate data for complex kernels")
    parser.add_argument("-c", "--cfg", type=pathlib.Path, required=True, help="Param config file")
    args = parser.parse_args()

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    kernel = param["kernel"]
    prec = param["prec"]
    
    # Determine PyTorch dtype based on precision
    # Note: We use double/float for calculations to ensure Golden result accuracy,
    # then cast to target precision string representation.
    if prec == 64:
        dtype = torch.float64
        cdtype = torch.complex128
    else:
        # For 32 and 16, we use float32 for generation to avoid PyTorch half-complex limitations
        dtype = torch.float32 
        cdtype = torch.complex64

    emit_str = (
        "// Generated by gen_data.py\n"
    )

    # -------------------------------------------
    # 1. CAXPY: Y = alpha * X + Y
    # -------------------------------------------
    if kernel == "CAXPY":
        M = param["M"]
        X = rand_complex((M,), dtype)
        Y = rand_complex((M,), dtype)
        alpha = rand_complex((1,), dtype)
        
        # Golden Calc
        result = alpha * X + Y
        
        emit_str += emit_caxpy(
            "caxpy", M=M, prec=prec, 
            X=X, Y=Y, alpha=alpha, result=result
        )
        outfile = "data_caxpy.h"

    # -------------------------------------------
    # 2. CDOTP: Res = sum(X * Y)
    # -------------------------------------------
    elif kernel == "CDOTP":
        M = param["M"]
        X = rand_complex((M,), dtype)
        Y = rand_complex((M,), dtype)
        
        # Golden Calc (Unconjugated Dot Product to match C code)
        # torch.dot performs Conjugated dot product for complex!
        # We need sum(X * Y) not sum(conj(X) * Y)
        result = torch.sum(X * Y)
        
        emit_str += emit_cdotp(
            "cdotp", M=M, prec=prec,
            X=X, Y=Y, result=result
        )
        outfile = "data_cdotp.h"

    # -------------------------------------------
    # 3/4/5. CMATMUL: C += A * B
    # -------------------------------------------
    elif kernel == "CMATMUL":
        M, N, K = param["M"], param["N"], param["K"]
        A = rand_complex((M, K), dtype)
        B = rand_complex((K, N), dtype)
        C = rand_complex((M, N), dtype)
        
        # Golden Calc: C_final = C_initial + (A @ B)
        # Note: PyTorch matmul is safe (no conjugation on operands implicitly)
        result = C + torch.matmul(A, B)
        
        emit_str += emit_cmatmul(
            "cmatmul", M=M, N=N, K=K, prec=prec,
            A=A, B=B, C=C, result=result
        )
        outfile = "data_cmatmul.h"

    else:
        print(f"Unknown kernel: {kernel}")
        return

    # Write File
    out_path = pathlib.Path(__file__).parent.parent / "data" / outfile
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w") as f:
        f.write(emit_str)
    
    print(f"Generated {outfile} for Kernel={kernel}, Prec={prec}")

if __name__ == "__main__":
    main()