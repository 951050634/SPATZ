#include "hp-cmatmul.h"
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct
{
    _Float16 real;
    _Float16 imag;
} chalf;

void matmul(chalf *c, const chalf *a, const chalf *b, const unsigned int M,
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
// 2xVL (Half Precision Complex)
// LMUL=4, SEW=16
// ---------------
void matmul_2xVL(chalf *c, const chalf *a, const chalf *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end)
{
    unsigned int p = p_start;
    while (p < p_end)
    {
        size_t gvl;
        // e16, m4
        asm volatile("vsetvli %[gvl], %[vl], e16, m4, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const chalf *b_ = b + p;
        chalf *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 2)
        {
            const chalf *a_ = a + m * N;
            const chalf *a__ = a_;

            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v12, 0");

            const chalf *b__ = b_;
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(b__));
            b__ += P;

            _Float16 ar0, ai0, ar1, ai1;
            ar0 = a__[0].real;
            ai0 = a__[0].imag;
            ar1 = a__[N].real;
            ai1 = a__[N].imag;
            a__++;

            unsigned int n = 0;
            while (n < N)
            {
                if (n + 1 < N)
                {
                    asm volatile("vlseg2e16.v v24, (%0);" ::"r"(b__));
                    b__ += P;
                }

                asm volatile("vfmacc.vf v0, %0, v16" ::"f"(ar0));
                asm volatile("vfnmsac.vf v0, %0, v20" ::"f"(ai0));
                asm volatile("vfmacc.vf v4, %0, v20" ::"f"(ar0));
                asm volatile("vfmacc.vf v4, %0, v16" ::"f"(ai0));

                asm volatile("vfmacc.vf v8, %0, v16" ::"f"(ar1));
                asm volatile("vfnmsac.vf v8, %0, v20" ::"f"(ai1));
                asm volatile("vfmacc.vf v12, %0, v20" ::"f"(ar1));
                asm volatile("vfmacc.vf v12, %0, v16" ::"f"(ai1));

                n++;
                if (n == N)
                    break;

                ar0 = a__[0].real;
                ai0 = a__[0].imag;
                ar1 = a__[N].real;
                ai1 = a__[N].imag;
                a__++;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e16.v v16, (%0);" ::"r"(b__));
                    b__ += P;
                }

                asm volatile("vfmacc.vf v0, %0, v24" ::"f"(ar0));
                asm volatile("vfnmsac.vf v0, %0, v28" ::"f"(ai0));
                asm volatile("vfmacc.vf v4, %0, v28" ::"f"(ar0));
                asm volatile("vfmacc.vf v4, %0, v24" ::"f"(ai0));

                asm volatile("vfmacc.vf v8, %0, v24" ::"f"(ar1));
                asm volatile("vfnmsac.vf v8, %0, v28" ::"f"(ai1));
                asm volatile("vfmacc.vf v12, %0, v28" ::"f"(ar1));
                asm volatile("vfmacc.vf v12, %0, v24" ::"f"(ai1));

                n++;
            }

            chalf *c_ptr_row0 = c_ + m * P;
            chalf *c_ptr_row1 = c_ + (m + 1) * P;

            // Row 0 Post-Accumulate
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(c_ptr_row0));
            asm volatile("vfadd.vv v0, v0, v16");
            asm volatile("vfadd.vv v4, v4, v20");
            asm volatile("vsseg2e16.v v0, (%0);" ::"r"(c_ptr_row0));

            // Row 1 Post-Accumulate
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(c_ptr_row1));
            asm volatile("vfadd.vv v8, v8, v16");
            asm volatile("vfadd.vv v12, v12, v20");
            asm volatile("vsseg2e16.v v8, (%0);" ::"r"(c_ptr_row1));
        }

        p += gvl;
    }
}

// ---------------
// 4xVL (Half Precision Complex)
// LMUL=2, SEW=16
// ---------------
void matmul_4xVL(chalf *c, const chalf *a, const chalf *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end)
{
    unsigned int p = p_start;
    while (p < p_end)
    {
        size_t gvl;
        // e16, m2
        asm volatile("vsetvli %[gvl], %[vl], e16, m2, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const chalf *b_ = b + p;
        chalf *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 4)
        {
            // Init Acc
            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v2, 0");
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v6, 0");
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v10, 0");
            asm volatile("vmv.v.i v12, 0");
            asm volatile("vmv.v.i v14, 0");

            const chalf *b__ = b_;
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(b__));
            b__ += P;

            unsigned int n = 0;
            while (n < N)
            {
                _Float16 ar[4], ai[4];
                ar[0] = a[(m + 0) * N + n].real;
                ai[0] = a[(m + 0) * N + n].imag;
                ar[1] = a[(m + 1) * N + n].real;
                ai[1] = a[(m + 1) * N + n].imag;
                ar[2] = a[(m + 2) * N + n].real;
                ai[2] = a[(m + 2) * N + n].imag;
                ar[3] = a[(m + 3) * N + n].real;
                ai[3] = a[(m + 3) * N + n].imag;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e16.v v20, (%0);" ::"r"(b__));
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
                ar[0] = a[(m + 0) * N + n].real;
                ai[0] = a[(m + 0) * N + n].imag;
                ar[1] = a[(m + 1) * N + n].real;
                ai[1] = a[(m + 1) * N + n].imag;
                ar[2] = a[(m + 2) * N + n].real;
                ai[2] = a[(m + 2) * N + n].imag;
                ar[3] = a[(m + 3) * N + n].real;
                ai[3] = a[(m + 3) * N + n].imag;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e16.v v16, (%0);" ::"r"(b__));
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

            chalf *c__ = c_ + m * P;

            // Row 0
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v0, v0, v16");
            asm volatile("vfadd.vv v2, v2, v18");
            asm volatile("vsseg2e16.v v0, (%0);" ::"r"(c__));
            c__ += P;

            // Row 1
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v4, v4, v16");
            asm volatile("vfadd.vv v6, v6, v18");
            asm volatile("vsseg2e16.v v4, (%0);" ::"r"(c__));
            c__ += P;

            // Row 2
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v8, v8, v16");
            asm volatile("vfadd.vv v10, v10, v18");
            asm volatile("vsseg2e16.v v8, (%0);" ::"r"(c__));
            c__ += P;

            // Row 3
            asm volatile("vlseg2e16.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v12, v12, v16");
            asm volatile("vfadd.vv v14, v14, v18");
            asm volatile("vsseg2e16.v v12, (%0);" ::"r"(c__));
        }

        p += gvl;
    }
}