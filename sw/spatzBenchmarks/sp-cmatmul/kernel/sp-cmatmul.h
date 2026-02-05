#ifndef DPFMATMUL_H
#define DPFMATMUL_H

void matmul(cfloat *c, const cfloat *a, const cfloat *b, const unsigned int M,
            const unsigned int N, const unsigned int P);

inline void matmul_2xVL(cfloat *c, const cfloat *a, const cfloat *b,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

inline void matmul_4xVL(cfloat *c, const cfloat *a, const cfloat *b,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

#endif