#include "sp-cmatmul.h"
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct
{
    float real;
    float imag;
} cfloat;

void matmul(cfloat *c, const cfloat *a, const cfloat *b, const unsigned int M,
            const unsigned int N, const unsigned int P)
{
    if (M <= 4)
    {
        matmul_2xVL(c, a, b, 0, M, N, P, 0, P);
    }
    else
    {
        matmul_4xVL(c, a, b, 0, M, N, P, 0, P);
    }
}

// ---------------
// 2xVL (Single Precision Complex)
// LMUL=4, SEW=32
// Optimization: Dual Pointers for A
// ---------------
void matmul_2xVL(cfloat *c, const cfloat *a, const cfloat *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end)
{
    unsigned int p = p_start;
    while (p < p_end)
    {
        size_t gvl;
        asm volatile("vsetvli %[gvl], %[vl], e32, m4, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const cfloat *b_ = b + p;
        cfloat *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 2)
        {
            // --- Pointer Optimization ---
            // 建立两个独立的指针，分别指向第 m 行和第 m+1 行
            const cfloat *p_a0 = a + m * N;
            const cfloat *p_a1 = p_a0 + N;

            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v12, 0");

            const cfloat *b__ = b_;
            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(b__));
            b__ += P;

            float ar0, ai0, ar1, ai1;

            // Prefetch Scalars (Direct dereference)
            ar0 = p_a0->real;
            ai0 = p_a0->imag;
            p_a0++;
            ar1 = p_a1->real;
            ai1 = p_a1->imag;
            p_a1++;

            unsigned int n = 0;
            while (n < N)
            {
                if (n + 1 < N)
                {
                    asm volatile("vlseg2e32.v v24, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // Row 0
                asm volatile("vfmacc.vf v0, %0, v16" ::"f"(ar0));
                asm volatile("vfnmsac.vf v0, %0, v20" ::"f"(ai0));
                asm volatile("vfmacc.vf v4, %0, v20" ::"f"(ar0));
                asm volatile("vfmacc.vf v4, %0, v16" ::"f"(ai0));

                // Row 1
                asm volatile("vfmacc.vf v8, %0, v16" ::"f"(ar1));
                asm volatile("vfnmsac.vf v8, %0, v20" ::"f"(ai1));
                asm volatile("vfmacc.vf v12, %0, v20" ::"f"(ar1));
                asm volatile("vfmacc.vf v12, %0, v16" ::"f"(ai1));

                n++;
                if (n == N)
                    break;

                // Load Next Scalars (No complex offset calculation)
                ar0 = p_a0->real;
                ai0 = p_a0->imag;
                p_a0++;
                ar1 = p_a1->real;
                ai1 = p_a1->imag;
                p_a1++;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e32.v v16, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // Pipeline Row 0
                asm volatile("vfmacc.vf v0, %0, v24" ::"f"(ar0));
                asm volatile("vfnmsac.vf v0, %0, v28" ::"f"(ai0));
                asm volatile("vfmacc.vf v4, %0, v28" ::"f"(ar0));
                asm volatile("vfmacc.vf v4, %0, v24" ::"f"(ai0));

                // Pipeline Row 1
                asm volatile("vfmacc.vf v8, %0, v24" ::"f"(ar1));
                asm volatile("vfnmsac.vf v8, %0, v28" ::"f"(ai1));
                asm volatile("vfmacc.vf v12, %0, v28" ::"f"(ar1));
                asm volatile("vfmacc.vf v12, %0, v24" ::"f"(ai1));

                n++;
            }

            // Write-Back
            cfloat *c_ptr_row0 = c_ + m * P;
            cfloat *c_ptr_row1 = c_ + (m + 1) * P;

            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(c_ptr_row0));
            asm volatile("vfadd.vv v0, v0, v16");
            asm volatile("vfadd.vv v4, v4, v20");
            asm volatile("vsseg2e32.v v0, (%0);" ::"r"(c_ptr_row0));

            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(c_ptr_row1));
            asm volatile("vfadd.vv v8, v8, v16");
            asm volatile("vfadd.vv v12, v12, v20");
            asm volatile("vsseg2e32.v v8, (%0);" ::"r"(c_ptr_row1));
        }

        p += gvl;
    }
}

// ---------------
// 4xVL (Single Precision Complex)
// LMUL=2, SEW=32
// Optimization: Quad Pointers for A
// ---------------
void matmul_4xVL(cfloat *c, const cfloat *a, const cfloat *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end)
{
    unsigned int p = p_start;
    while (p < p_end)
    {
        size_t gvl;
        asm volatile("vsetvli %[gvl], %[vl], e32, m2, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const cfloat *b_ = b + p;
        cfloat *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 4)
        {
            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v2, 0");
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v6, 0");
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v10, 0");
            asm volatile("vmv.v.i v12, 0");
            asm volatile("vmv.v.i v14, 0");

            // --- Pointer Optimization ---
            const cfloat *p_a0 = a + m * N;
            const cfloat *p_a1 = p_a0 + N;
            const cfloat *p_a2 = p_a1 + N;
            const cfloat *p_a3 = p_a2 + N;

            const cfloat *b__ = b_;
            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(b__));
            b__ += P;

            unsigned int n = 0;
            while (n < N)
            {
                float ar[4], ai[4];

                // Scalar Load (Quad Pointer)
                ar[0] = p_a0->real;
                ai[0] = p_a0->imag;
                p_a0++;
                ar[1] = p_a1->real;
                ai[1] = p_a1->imag;
                p_a1++;
                ar[2] = p_a2->real;
                ai[2] = p_a2->imag;
                p_a2++;
                ar[3] = p_a3->real;
                ai[3] = p_a3->imag;
                p_a3++;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e32.v v20, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // Block 1 (v16/v18)
                // Row 0
                asm volatile("vfmacc.vf v0, %0, v16" ::"f"(ar[0]));
                asm volatile("vfnmsac.vf v0, %0, v18" ::"f"(ai[0]));
                asm volatile("vfmacc.vf v2, %0, v18" ::"f"(ar[0]));
                asm volatile("vfmacc.vf v2, %0, v16" ::"f"(ai[0]));
                // Row 1
                asm volatile("vfmacc.vf v4, %0, v16" ::"f"(ar[1]));
                asm volatile("vfnmsac.vf v4, %0, v18" ::"f"(ai[1]));
                asm volatile("vfmacc.vf v6, %0, v18" ::"f"(ar[1]));
                asm volatile("vfmacc.vf v6, %0, v16" ::"f"(ai[1]));
                // Row 2
                asm volatile("vfmacc.vf v8, %0, v16" ::"f"(ar[2]));
                asm volatile("vfnmsac.vf v8, %0, v18" ::"f"(ai[2]));
                asm volatile("vfmacc.vf v10, %0, v18" ::"f"(ar[2]));
                asm volatile("vfmacc.vf v10, %0, v16" ::"f"(ai[2]));
                // Row 3
                asm volatile("vfmacc.vf v12, %0, v16" ::"f"(ar[3]));
                asm volatile("vfnmsac.vf v12, %0, v18" ::"f"(ai[3]));
                asm volatile("vfmacc.vf v14, %0, v18" ::"f"(ar[3]));
                asm volatile("vfmacc.vf v14, %0, v16" ::"f"(ai[3]));

                n++;
                if (n == N)
                    break;

                // Load Next Scalars
                ar[0] = p_a0->real;
                ai[0] = p_a0->imag;
                p_a0++;
                ar[1] = p_a1->real;
                ai[1] = p_a1->imag;
                p_a1++;
                ar[2] = p_a2->real;
                ai[2] = p_a2->imag;
                p_a2++;
                ar[3] = p_a3->real;
                ai[3] = p_a3->imag;
                p_a3++;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e32.v v16, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // Block 2 (v20/v22)
                // Row 0
                asm volatile("vfmacc.vf v0, %0, v20" ::"f"(ar[0]));
                asm volatile("vfnmsac.vf v0, %0, v22" ::"f"(ai[0]));
                asm volatile("vfmacc.vf v2, %0, v22" ::"f"(ar[0]));
                asm volatile("vfmacc.vf v2, %0, v20" ::"f"(ai[0]));
                // Row 1
                asm volatile("vfmacc.vf v4, %0, v20" ::"f"(ar[1]));
                asm volatile("vfnmsac.vf v4, %0, v22" ::"f"(ai[1]));
                asm volatile("vfmacc.vf v6, %0, v22" ::"f"(ar[1]));
                asm volatile("vfmacc.vf v6, %0, v20" ::"f"(ai[1]));
                // Row 2
                asm volatile("vfmacc.vf v8, %0, v20" ::"f"(ar[2]));
                asm volatile("vfnmsac.vf v8, %0, v22" ::"f"(ai[2]));
                asm volatile("vfmacc.vf v10, %0, v22" ::"f"(ar[2]));
                asm volatile("vfmacc.vf v10, %0, v20" ::"f"(ai[2]));
                // Row 3
                asm volatile("vfmacc.vf v12, %0, v20" ::"f"(ar[3]));
                asm volatile("vfnmsac.vf v12, %0, v22" ::"f"(ai[3]));
                asm volatile("vfmacc.vf v14, %0, v22" ::"f"(ar[3]));
                asm volatile("vfmacc.vf v14, %0, v20" ::"f"(ai[3]));

                n++;
            }

            // Write-Back
            cfloat *c__ = c_ + m * P;
            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v0, v0, v16");
            asm volatile("vfadd.vv v2, v2, v18");
            asm volatile("vsseg2e32.v v0, (%0);" ::"r"(c__));
            c__ += P;

            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v4, v4, v16");
            asm volatile("vfadd.vv v6, v6, v18");
            asm volatile("vsseg2e32.v v4, (%0);" ::"r"(c__));
            c__ += P;

            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v8, v8, v16");
            asm volatile("vfadd.vv v10, v10, v18");
            asm volatile("vsseg2e32.v v8, (%0);" ::"r"(c__));
            c__ += P;

            asm volatile("vlseg2e32.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v12, v12, v16");
            asm volatile("vfadd.vv v14, v14, v18");
            asm volatile("vsseg2e32.v v12, (%0);" ::"r"(c__));
        }
        p += gvl;
    }
}