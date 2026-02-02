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

/*
考虑到以下问题：
1、覆盖写 or 累加写:
    我们需要确保最终结果是 C = C + AB，而不是仅仅 C = AB（这是为了对应上大矩阵的分块计算操作）
    因此，在写回结果时，我们需要先加载旧的 C 值，加上累加器的值，再存回去。
2、寄存器分配:
    采用循环后累加的操作，把计算增量AB和累加C+=AB两步分开处理
*/
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
            while (n < N) // 此处循环计算的过程中无法加载C，因为寄存器完全被占用
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

void cmatmul_4xVL(cdouble *c, const cdouble *a, const cdouble *b,
                  const unsigned int m_start, const unsigned int m_end,
                  const unsigned int N, const unsigned int P,
                  const unsigned int p_start, const unsigned int p_end)
{

    unsigned int p = p_start;
    while (p < p_end)
    {
        // 1. 设置向量长度 (LMUL=2)
        size_t gvl;
        asm volatile("vsetvli %[gvl], %[vl], e64, m2, ta, ma"
                     : [gvl] "=r"(gvl)
                     : [vl] "r"(p_end - p));

        const cdouble *b_ = b + p;
        cdouble *c_ = c + p;

        for (unsigned int m = m_start; m < m_end; m += 4)
        {
            // 2. 初始化 4 行的累加器为 0
            // 虽然我们要计算 C += AB，但为了寄存器分配方便，
            // 我们先计算 Delta = AB (从0开始累加)，最后再加回 C。

            // Row 0
            asm volatile("vmv.v.i v0, 0");
            asm volatile("vmv.v.i v2, 0");
            // Row 1
            asm volatile("vmv.v.i v4, 0");
            asm volatile("vmv.v.i v6, 0");
            // Row 2
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v10, 0");
            // Row 3
            asm volatile("vmv.v.i v12, 0");
            asm volatile("vmv.v.i v14, 0");

            // 3. 流水线准备 (Pipeline Prologue)
            const cdouble *b__ = b_;
            // Load first B chunk into v16(Re), v18(Im)
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(b__));
            b__ += P;

            unsigned int n = 0;
            while (n < N)
            {
                // Load Scalars for 4 rows
                // 实际高性能代码可能需要手动展开这个循环以减少标量负载开销
                double ar[4], ai[4];
                // Row m
                ar[0] = a[(m + 0) * N + n].real;
                ai[0] = a[(m + 0) * N + n].imag;
                // Row m+1
                ar[1] = a[(m + 1) * N + n].real;
                ai[1] = a[(m + 1) * N + n].imag;
                // Row m+2
                ar[2] = a[(m + 2) * N + n].real;
                ai[2] = a[(m + 2) * N + n].imag;
                // Row m+3
                ar[3] = a[(m + 3) * N + n].real;
                ai[3] = a[(m + 3) * N + n].imag;

                // Pipeline Prefetch: Load Next B into v20/v22
                if (n + 1 < N)
                {
                    asm volatile("vlseg2e64.v v20, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // --- Compute Block 1 (Using v16/v18) ---

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

                // --- Pipeline Block 2 (Using v20/v22) ---
                // Load Next Scalars
                ar[0] = a[(m + 0) * N + n].real;
                ai[0] = a[(m + 0) * N + n].imag;
                ar[1] = a[(m + 1) * N + n].real;
                ai[1] = a[(m + 1) * N + n].imag;
                ar[2] = a[(m + 2) * N + n].real;
                ai[2] = a[(m + 2) * N + n].imag;
                ar[3] = a[(m + 3) * N + n].real;
                ai[3] = a[(m + 3) * N + n].imag;

                // Pipeline Back-load: Next-Next B into v16/v18
                if (n + 1 < N)
                {
                    asm volatile("vlseg2e64.v v16, (%0);" ::"r"(b__));
                    b__ += P;
                }

                // Compute Row 0
                asm volatile("vfmacc.vf v0, %0, v20" ::"f"(ar[0]));
                asm volatile("vfnmsac.vf v0, %0, v22" ::"f"(ai[0]));
                asm volatile("vfmacc.vf v2, %0, v22" ::"f"(ar[0]));
                asm volatile("vfmacc.vf v2, %0, v20" ::"f"(ai[0]));
                // Compute Row 1
                asm volatile("vfmacc.vf v4, %0, v20" ::"f"(ar[1]));
                asm volatile("vfnmsac.vf v4, %0, v22" ::"f"(ai[1]));
                asm volatile("vfmacc.vf v6, %0, v22" ::"f"(ar[1]));
                asm volatile("vfmacc.vf v6, %0, v20" ::"f"(ai[1]));
                // Compute Row 2
                asm volatile("vfmacc.vf v8, %0, v20" ::"f"(ar[2]));
                asm volatile("vfnmsac.vf v8, %0, v22" ::"f"(ai[2]));
                asm volatile("vfmacc.vf v10, %0, v22" ::"f"(ar[2]));
                asm volatile("vfmacc.vf v10, %0, v20" ::"f"(ai[2]));
                // Compute Row 3
                asm volatile("vfmacc.vf v12, %0, v20" ::"f"(ar[3]));
                asm volatile("vfnmsac.vf v12, %0, v22" ::"f"(ai[3]));
                asm volatile("vfmacc.vf v14, %0, v22" ::"f"(ar[3]));
                asm volatile("vfmacc.vf v14, %0, v20" ::"f"(ai[3]));

                n++;
            }

            // 4. Write-Back Consistency Phase (Post-Accumulation)
            // Loop is done. v16-v23 are now free (garbage data).
            // We reuse v16, v18 as temporary storage for Old C.

            cdouble *c__ = c_ + m * P;

            // --- Row 0 ---
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(c__)); // Load Old C -> v16, v18
            asm volatile("vfadd.vv v0, v0, v16");              // Acc_Re += Old_Re
            asm volatile("vfadd.vv v2, v2, v18");              // Acc_Im += Old_Im
            asm volatile("vsseg2e64.v v0, (%0);" ::"r"(c__));  // Store Back
            c__ += P;

            // --- Row 1 ---
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v4, v4, v16");
            asm volatile("vfadd.vv v6, v6, v18");
            asm volatile("vsseg2e64.v v4, (%0);" ::"r"(c__));
            c__ += P;

            // --- Row 2 ---
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v8, v8, v16");
            asm volatile("vfadd.vv v10, v10, v18");
            asm volatile("vsseg2e64.v v8, (%0);" ::"r"(c__));
            c__ += P;

            // --- Row 3 ---
            asm volatile("vlseg2e64.v v16, (%0);" ::"r"(c__));
            asm volatile("vfadd.vv v12, v12, v16");
            asm volatile("vfadd.vv v14, v14, v18");
            asm volatile("vsseg2e64.v v12, (%0);" ::"r"(c__));
        }

        p += gvl;
    }
}