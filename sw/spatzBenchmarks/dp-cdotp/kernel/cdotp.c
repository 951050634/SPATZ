#include "cdotp.h"

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

// 64-bit Complex Dot Product
// sum += x[i] * y[i]
cdouble cdotp_v64b(const cdouble *x, const cdouble *y, unsigned int avl)
{
    unsigned int vl;
    double red_real, red_imag;

    // INIT: LMUL=4
    // v16-v19: Real Accumulator
    // v20-v23: Imag Accumulator
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vmv.v.i v16, 0"); // Acc_Real = 0
    asm volatile("vmv.v.i v20, 0"); // Acc_Imag = 0

    do
    {
        asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

        // Load X: v0=Xr, v4=Xi
        asm volatile("vlseg2e64.v v0, (%0)" ::"r"(x));

        // Load Y: v8=Yr, v12=Yi
        asm volatile("vlseg2e64.v v8, (%0)" ::"r"(y));

        // (Xr + iXi) * (Yr + iYi) = (XrYr - XiYi) + i(XrYi + XiYr)
        // Real Part: Acc_r += Xr*Yr - Xi*Yi
        asm volatile("vfmacc.vv v16, v0, v8");   // v16 += Xr * Yr
        asm volatile("vfnmsac.vv v16, v4, v12"); // v16 -= Xi * Yi
        // Imag Part: Acc_i += Xr*Yi + Xi*Yr
        asm volatile("vfmacc.vv v20, v0, v12"); // v20 += Xr * Yi
        asm volatile("vfmacc.vv v20, v4, v8");  // v20 += Xi * Yr

        x += vl;
        y += vl;
        avl -= vl;
    } while (avl > 0);

    asm volatile("vmv.s.x v24, zero");
    asm volatile("vfredusum.vs v16, v16, v24");
    asm volatile("vfmv.f.s %0, v16" : "=f"(red_real));
    asm volatile("vfredusum.vs v20, v20, v24");
    asm volatile("vfmv.f.s %0, v20" : "=f"(red_imag));
    cdouble res = {red_real, red_imag};
    return res;
}

cfloat cdotp_v32b(const cfloat *x, const cfloat *y, unsigned int avl)
{
    unsigned int vl;
    float red_real, red_imag;

    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vmv.v.i v16, 0");
    asm volatile("vmv.v.i v20, 0");

    do
    {
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

        asm volatile("vlseg2e32.v v0, (%0)" ::"r"(x)); // v0=Xr, v4=Xi
        asm volatile("vlseg2e32.v v8, (%0)" ::"r"(y)); // v8=Yr, v12=Yi

        asm volatile("vfmacc.vv v16, v0, v8");
        asm volatile("vfnmsac.vv v16, v4, v12");
        asm volatile("vfmacc.vv v20, v0, v12");
        asm volatile("vfmacc.vv v20, v4, v8");

        x += vl;
        y += vl;
        avl -= vl;
    } while (avl > 0);

    asm volatile("vmv.s.x v24, zero"); // Clear base
    asm volatile("vfredusum.vs v16, v16, v24");
    asm volatile("vfmv.f.s %0, v16" : "=f"(red_real));
    asm volatile("vfredusum.vs v20, v20, v24");
    asm volatile("vfmv.f.s %0, v20" : "=f"(red_imag));

    cfloat res = {red_real, red_imag};
    return res;
}

chalf cdotp_v16b(const chalf *x, const chalf *y, unsigned int avl)
{
    unsigned int vl;
    _Float16 red_real, red_imag;

    asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vmv.v.i v16, 0");
    asm volatile("vmv.v.i v20, 0");

    do
    {
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));

        asm volatile("vlseg2e16.v v0, (%0)" ::"r"(x));
        asm volatile("vlseg2e16.v v8, (%0)" ::"r"(y));

        asm volatile("vfmacc.vv v16, v0, v8");
        asm volatile("vfnmsac.vv v16, v4, v12");
        asm volatile("vfmacc.vv v20, v0, v12");
        asm volatile("vfmacc.vv v20, v4, v8");

        x += vl;
        y += vl;
        avl -= vl;
    } while (avl > 0);

    asm volatile("vmv.s.x v24, zero");
    asm volatile("vfredusum.vs v16, v16, v24");
    asm volatile("vfmv.f.s %0, v16" : "=f"(red_real));
    asm volatile("vfredusum.vs v20, v20, v24");
    asm volatile("vfmv.f.s %0, v20" : "=f"(red_imag));

    chalf res = {red_real, red_imag};
    return res;
}