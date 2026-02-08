#include "widening-cmatmul.h"
#include <stddef.h>

void matmul(cbyte *c, const cbyte *a, const cbyte *b, const unsigned int M,
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

// -----------------------------------------------------------
// 2xVL Widening Kernel (Fix: Register Alignment for m2)
// Input: e8, m2 (Narrow)
// Acc:   e16, m4 (Wide)
// -----------------------------------------------------------
void matmul_2xVL(cbyte *c, const cbyte *a, const cbyte *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end)
{

    unsigned int p = p_start;
    while (p < p_end)
    {
        size_t gvl;
        asm volatile("vsetvli %[gvl], %[vl], e8, m2, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const cbyte *b_ = b + p;
        cbyte *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 2)
        {
            const cbyte *p_a0 = a + m * N;
            const cbyte *p_a1 = p_a0 + N;

            asm volatile("vsetvli zero, %0, e16, m4, ta, ma" ::"r"(gvl));
            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v12, 0");

            asm volatile("vsetvli zero, %0, e8, m2, ta, ma" ::"r"(gvl));

            const cbyte *b__ = b_;
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(b__));
            b__ += P;

            double ar0, ai0, ar1, ai1;
            asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar0) : [a] "r"(p_a0));
            asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai0) : [a] "r"(p_a0));
            p_a0++;
            asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar1) : [a] "r"(p_a1));
            asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai1) : [a] "r"(p_a1));
            p_a1++;

            unsigned int n = 0;
            while (n < N)
            {
                if (n + 1 < N)
                {
                    asm volatile("vlseg2e8.v v20, (%0);" ::"r"(b__));
                    b__ += P;
                }

                asm volatile("vfwmacc.vf v0, %0, v16" ::"f"(ar0));
                asm volatile("vfwnmsac.vf v0, %0, v18" ::"f"(ai0));
                asm volatile("vfwmacc.vf v4, %0, v18" ::"f"(ar0));
                asm volatile("vfwmacc.vf v4, %0, v16" ::"f"(ai0));

                asm volatile("vfwmacc.vf v8, %0, v16" ::"f"(ar1));
                asm volatile("vfwnmsac.vf v8, %0, v18" ::"f"(ai1));
                asm volatile("vfwmacc.vf v12, %0, v18" ::"f"(ar1));
                asm volatile("vfwmacc.vf v12, %0, v16" ::"f"(ai1));

                n++;
                if (n == N)
                    break;

                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar0) : [a] "r"(p_a0));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai0) : [a] "r"(p_a0));
                p_a0++;
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar1) : [a] "r"(p_a1));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai1) : [a] "r"(p_a1));
                p_a1++;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e8.v v16, (%0);" ::"r"(b__));
                    b__ += P;
                }

                asm volatile("vfwmacc.vf v0, %0, v20" ::"f"(ar0));
                asm volatile("vfwnmsac.vf v0, %0, v22" ::"f"(ai0));
                asm volatile("vfwmacc.vf v4, %0, v22" ::"f"(ar0));
                asm volatile("vfwmacc.vf v4, %0, v20" ::"f"(ai0));

                asm volatile("vfwmacc.vf v8, %0, v20" ::"f"(ar1));
                asm volatile("vfwnmsac.vf v8, %0, v22" ::"f"(ai1));
                asm volatile("vfwmacc.vf v12, %0, v22" ::"f"(ar1));
                asm volatile("vfwmacc.vf v12, %0, v20" ::"f"(ai1));

                n++;
            }

            // ---------------------------------------------------------
            // Write-Back with Narrowing (Corrected Register Alignment)
            // ---------------------------------------------------------
            cbyte *c_ptr_row0 = c_ + m * P;
            cbyte *c_ptr_row1 = c_ + (m + 1) * P;

            // Row 0
            // v0 (m4) -> v24 (m2). v4 (m4) -> v26 (m2).
            asm volatile("vfncvt.f.f.w v24, v0");
            asm volatile("vfncvt.f.f.w v26, v4");
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(c_ptr_row0));
            asm volatile("vfadd.vv v24, v24, v16");
            asm volatile("vfadd.vv v26, v26, v18");
            asm volatile("vsseg2e8.v v24, (%0);" ::"r"(c_ptr_row0));

            // Row 1
            // v8 (m4) -> v28 (m2). v12 (m4) -> v30 (m2).
            asm volatile("vfncvt.f.f.w v28, v8");
            asm volatile("vfncvt.f.f.w v30, v12");
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(c_ptr_row1));
            asm volatile("vfadd.vv v28, v28, v16");
            asm volatile("vfadd.vv v30, v30, v18");
            asm volatile("vsseg2e8.v v28, (%0);" ::"r"(c_ptr_row1));
        }
        p += gvl;
    }
}

// 4xVL Widening Kernel (Input e8, m1) - Indices v24, v25 are valid for m1
void matmul_4xVL(cbyte *c, const cbyte *a, const cbyte *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end)
{

    unsigned int p = p_start;
    while (p < p_end)
    {
        size_t gvl;
        asm volatile("vsetvli %[gvl], %[vl], e8, m1, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const cbyte *b_ = b + p;
        cbyte *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 4)
        {
            asm volatile("vsetvli zero, %0, e16, m2, ta, ma" ::"r"(gvl));
            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v2, 0");
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v6, 0");
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v10, 0");
            asm volatile("vmv.v.i v12, 0");
            asm volatile("vmv.v.i v14, 0");

            asm volatile("vsetvli zero, %0, e8, m1, ta, ma" ::"r"(gvl));

            const cbyte *p_a0 = a + m * N;
            const cbyte *p_a1 = p_a0 + N;
            const cbyte *p_a2 = p_a1 + N;
            const cbyte *p_a3 = p_a2 + N;

            const cbyte *b__ = b_;
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(b__));
            b__ += P;

            unsigned int n = 0;
            while (n < N)
            {
                double ar[4], ai[4];
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[0]) : [a] "r"(p_a0));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[0]) : [a] "r"(p_a0));
                p_a0++;
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[1]) : [a] "r"(p_a1));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[1]) : [a] "r"(p_a1));
                p_a1++;
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[2]) : [a] "r"(p_a2));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[2]) : [a] "r"(p_a2));
                p_a2++;
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[3]) : [a] "r"(p_a3));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[3]) : [a] "r"(p_a3));
                p_a3++;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e8.v v20, (%0);" ::"r"(b__));
                    b__ += P;
                }

                asm volatile("vfwmacc.vf v0, %0, v16" ::"f"(ar[0]));
                asm volatile("vfwnmsac.vf v0, %0, v17" ::"f"(ai[0]));
                asm volatile("vfwmacc.vf v2, %0, v17" ::"f"(ar[0]));
                asm volatile("vfwmacc.vf v2, %0, v16" ::"f"(ai[0]));

                asm volatile("vfwmacc.vf v4, %0, v16" ::"f"(ar[1]));
                asm volatile("vfwnmsac.vf v4, %0, v17" ::"f"(ai[1]));
                asm volatile("vfwmacc.vf v6, %0, v17" ::"f"(ar[1]));
                asm volatile("vfwmacc.vf v6, %0, v16" ::"f"(ai[1]));

                asm volatile("vfwmacc.vf v8, %0, v16" ::"f"(ar[2]));
                asm volatile("vfwnmsac.vf v8, %0, v17" ::"f"(ai[2]));
                asm volatile("vfwmacc.vf v10, %0, v17" ::"f"(ar[2]));
                asm volatile("vfwmacc.vf v10, %0, v16" ::"f"(ai[2]));

                asm volatile("vfwmacc.vf v12, %0, v16" ::"f"(ar[3]));
                asm volatile("vfwnmsac.vf v12, %0, v17" ::"f"(ai[3]));
                asm volatile("vfwmacc.vf v14, %0, v17" ::"f"(ar[3]));
                asm volatile("vfwmacc.vf v14, %0, v16" ::"f"(ai[3]));

                n++;
                if (n == N)
                    break;

                // Load Next Scalars
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[0]) : [a] "r"(p_a0));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[0]) : [a] "r"(p_a0));
                p_a0++;
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[1]) : [a] "r"(p_a1));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[1]) : [a] "r"(p_a1));
                p_a1++;
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[2]) : [a] "r"(p_a2));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[2]) : [a] "r"(p_a2));
                p_a2++;
                asm volatile("flb %[t], 0(%[a])" : [t] "=f"(ar[3]) : [a] "r"(p_a3));
                asm volatile("flb %[t], 1(%[a])" : [t] "=f"(ai[3]) : [a] "r"(p_a3));
                p_a3++;

                if (n + 1 < N)
                {
                    asm volatile("vlseg2e8.v v16, (%0);" ::"r"(b__));
                    b__ += P;
                }

                asm volatile("vfwmacc.vf v0, %0, v20" ::"f"(ar[0]));
                asm volatile("vfwnmsac.vf v0, %0, v21" ::"f"(ai[0]));
                asm volatile("vfwmacc.vf v2, %0, v21" ::"f"(ar[0]));
                asm volatile("vfwmacc.vf v2, %0, v20" ::"f"(ai[0]));

                asm volatile("vfwmacc.vf v4, %0, v20" ::"f"(ar[1]));
                asm volatile("vfwnmsac.vf v4, %0, v21" ::"f"(ai[1]));
                asm volatile("vfwmacc.vf v6, %0, v21" ::"f"(ar[1]));
                asm volatile("vfwmacc.vf v6, %0, v20" ::"f"(ai[1]));

                asm volatile("vfwmacc.vf v8, %0, v20" ::"f"(ar[2]));
                asm volatile("vfwnmsac.vf v8, %0, v21" ::"f"(ai[2]));
                asm volatile("vfwmacc.vf v10, %0, v21" ::"f"(ar[2]));
                asm volatile("vfwmacc.vf v10, %0, v20" ::"f"(ai[2]));

                asm volatile("vfwmacc.vf v12, %0, v20" ::"f"(ar[3]));
                asm volatile("vfwnmsac.vf v12, %0, v21" ::"f"(ai[3]));
                asm volatile("vfwmacc.vf v14, %0, v21" ::"f"(ar[3]));
                asm volatile("vfwmacc.vf v14, %0, v20" ::"f"(ai[3]));

                n++;
            }

            cbyte *c_ptr = c_ + m * P;

            // Row 0
            asm volatile("vfncvt.f.f.w v24, v0");
            asm volatile("vfncvt.f.f.w v25, v2");
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(c_ptr));
            asm volatile("vfadd.vv v24, v24, v16");
            asm volatile("vfadd.vv v25, v25, v17");
            asm volatile("vsseg2e8.v v24, (%0);" ::"r"(c_ptr));
            c_ptr += P;

            // Row 1
            asm volatile("vfncvt.f.f.w v24, v4");
            asm volatile("vfncvt.f.f.w v25, v6");
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(c_ptr));
            asm volatile("vfadd.vv v24, v24, v16");
            asm volatile("vfadd.vv v25, v25, v17");
            asm volatile("vsseg2e8.v v24, (%0);" ::"r"(c_ptr));
            c_ptr += P;

            // Row 2
            asm volatile("vfncvt.f.f.w v24, v8");
            asm volatile("vfncvt.f.f.w v25, v10");
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(c_ptr));
            asm volatile("vfadd.vv v24, v24, v16");
            asm volatile("vfadd.vv v25, v25, v17");
            asm volatile("vsseg2e8.v v24, (%0);" ::"r"(c_ptr));
            c_ptr += P;

            // Row 3
            asm volatile("vfncvt.f.f.w v24, v12");
            asm volatile("vfncvt.f.f.w v25, v14");
            asm volatile("vlseg2e8.v v16, (%0);" ::"r"(c_ptr));
            asm volatile("vfadd.vv v24, v24, v16");
            asm volatile("vfadd.vv v25, v25, v17");
            asm volatile("vsseg2e8.v v24, (%0);" ::"r"(c_ptr));
        }
        p += gvl;
    }
}