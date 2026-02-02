#include "dp-cmatmul.h"
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct 
{
    double real;
    double imag;
} cdouble;


void matmul(double *c, const double *a, const double *b, const unsigned int M,
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


void cmatmul_2xVL(cdouble *c, const cdouble *a, const cdouble *b,
                  const unsigned int m_start, const unsigned int m_end,
                  const unsigned int N, const unsigned int P,
                  const unsigned int p_start, const unsigned int p_end)
{
    unsigned int p = p_start;
    while (p < p_end)
    {
        size_t gvl;
        asm volatile("vsetvli %[gvl], %[vl], e64, m4, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const cdouble *b_ = b + p;
        cdouble *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 2)
        {
            const cdouble *a_ = a + m * N;
            const cdouble *a__ = a_;

            // 2. 初始化累加器为 0
            // 注意：这里仍然清零！因为我们在计算 "Delta" (A*B)。
            // 我们将在最后把 C 的原值加回来。这样可以避免在循环中占用寄存器。
            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v12, 0");

            // 3. 进入计算流水线 (代码与之前相同，旨在计算 A*B)
            const cdouble *b__ = b_;
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(b__));
            b__ += P;

            double ar0, ai0, ar1, ai1;
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
                    asm volatile("vlseg2e64.v v24, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // Row 0 Calc
                asm volatile("vfmacc.vf v0, %0, v16" ::"f"(ar0));
                asm volatile("vfnmsac.vf v0, %0, v20" ::"f"(ai0));
                asm volatile("vfmacc.vf v4, %0, v20" ::"f"(ar0));
                asm volatile("vfmacc.vf v4, %0, v16" ::"f"(ai0));

                // Row 1 Calc
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
                    asm volatile("vlseg2e64.v v16, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // Row 0 Calc (Pipeline)
                asm volatile("vfmacc.vf v0, %0, v24" ::"f"(ar0));
                asm volatile("vfnmsac.vf v0, %0, v28" ::"f"(ai0));
                asm volatile("vfmacc.vf v4, %0, v28" ::"f"(ar0));
                asm volatile("vfmacc.vf v4, %0, v24" ::"f"(ai0));

                // Row 1 Calc (Pipeline)
                asm volatile("vfmacc.vf v8, %0, v24" ::"f"(ar1));
                asm volatile("vfnmsac.vf v8, %0, v28" ::"f"(ai1));
                asm volatile("vfmacc.vf v12, %0, v28" ::"f"(ar1));
                asm volatile("vfmacc.vf v12, %0, v24" ::"f"(ai1));

                n++;
            }

            // -------------------------------------------------------------
            // 4. Write-Back Consistency (关键修改)
            // 现在循环结束，v16-v31 空闲。
            // 我们需要执行: C_new = Accumulator + C_old
            // -------------------------------------------------------------

            cdouble *c_ptr_row0 = c_ + m * P;
            cdouble *c_ptr_row1 = c_ + (m + 1) * P;

            // --- 处理 Row 0 ---
            // a. Load Old C into v16(Real), v20(Imag)
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(c_ptr_row0));

            // b. Accumulate: Acc = Acc + Old_C
            asm volatile("vfadd.vv v0, v0, v16"); // Real += Old_Real
            asm volatile("vfadd.vv v4, v4, v20"); // Imag += Old_Imag

            // c. Store Back
            asm volatile("vsseg2e64.v v0, (%0);" ::"r"(c_ptr_row0));

            // --- 处理 Row 1 ---
            // 复用 v16, v20 作为临时寄存器
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(c_ptr_row1));

            // Accumulate
            asm volatile("vfadd.vv v8, v8, v16");   // Row1 Real += Old
            asm volatile("vfadd.vv v12, v12, v20"); // Row1 Imag += Old

            // Store Back
            asm volatile("vsseg2e64.v v8, (%0);" ::"r"(c_ptr_row1));
        }

        p += gvl;
    }
}