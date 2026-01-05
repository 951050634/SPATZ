#include "caxpy.h"

typedef struct
{
    double real;
    double imag;
} cdouble;

// y = a * x + y for complex numbers
void caxpy_v64b(const cdouble a, const cdouble *x, cdouble *y, unsigned int avl)
{
    double ar = a.real;
    double ai = a.imag;
    unsigned int vl;

    do
    {
        // set the vl
        asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

        // load vectors
        asm volatile("vlseg2e64.v v0, (%0)" ::"r"(x));
        asm volatile("vlseg2e64.v v8, (%0)" ::"r"(y));

        // calculate
        // vd, rs1, vs2 # vd[i] += (f[rs1] * vs2[i])
        asm volatile("vfmacc.vf v8, %0, v0" ::"f"(ar));  // y_real += ar * x_real
        asm volatile("vfnmsac.vf v8, %0, v4" ::"f"(ai)); // y_real -= ai * x_imag
        asm volatile("vfmacc.vf v12, %0, v4" ::"f"(ar)); // y_imag += ar * x_imag
        asm volatile("vfmacc.vf v12, %0, v0" ::"f"(ai)); // y_imag += ai * x_real

        // store results
        asm volatile("vsseg2e64.v v8, (%0)" ::"r"(y));

        // pointer update
        x += vl;
        y += vl;
        avl -= vl;
    } while (avl > 0);
}

void caxpy_v32b(const cdouble a, const cdouble *x, cdouble *y, unsigned int avl)
{
    double ar = a.real;
    double ai = a.imag;
    unsigned int vl;

    do
    {
        // set the vl
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

        // load vectors
        asm volatile("vlseg2e32.v v0, (%0)" ::"r"(x));
        asm volatile("vlseg2e32.v v8, (%0)" ::"r"(y));
        // calculate
        // vd, rs1, vs2 # vd[i] += (f[rs1] * vs2[i])
        asm volatile("vfmacc.vf v8, %0, v0" ::"f"(ar));  // y_real += ar * x_real
        asm volatile("vfnmsac.vf v8, %0, v4" ::"f"(ai)); // y_real -= ai * x_imag
        asm volatile("vfmacc.vf v12, %0, v4" ::"f"(ar)); // y_imag += ar * x_imag
        asm volatile("vfmacc.vf v12, %0, v0" ::"f"(ai)); // y_imag += ai * x_real

        // store results
        asm volatile("vsseg2e32.v v8, (%0)" ::"r"(y));

        // pointer update
        x += vl;
        y += vl;
        avl -= vl;
    } while (avl > 0);
}

void caxpy_v16b(const cdouble a, const cdouble *x, cdouble *y, unsigned int avl)
{
    double ar = a.real;
    double ai = a.imag;
    unsigned int vl;

    do
    {
        // set the vl
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));

        // load vectors
        asm volatile("vlseg2e16.v v0, (%0)" ::"r"(x));
        asm volatile("vlseg2e16.v v8, (%0)" ::"r"(y));
        // calculate
        // vd, rs1, vs2 # vd[i] += (f[rs1] * vs2[i])
        asm volatile("vfmacc.vf v8, %0, v0" ::"f"(ar));  // y_real += ar * x_real
        asm volatile("vfnmsac.vf v8, %0, v4" ::"f"(ai)); // y_real -= ai * x_imag
        asm volatile("vfmacc.vf v12, %0, v4" ::"f"(ar)); // y_imag += ar * x_imag
        asm volatile("vfmacc.vf v12, %0, v0" ::"f"(ai)); // y_imag += ai * x_real

        // store results
        asm volatile("vsseg2e16.v v8, (%0)" ::"r"(y));

        // pointer update
        x += vl;
        y += vl;
        avl -= vl;
    } while (avl > 0);
}