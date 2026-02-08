#ifndef WIDENINGFMATMUL_H
#define WIDENINGFMATMUL_H

typedef struct
{
    _Float16 real;
    _Float16 imag;
} chalf;

void matmul(chalf *c, const chalf *a, const chalf *b, const unsigned int M,
            const unsigned int N, const unsigned int P);

inline void matmul_2xVL(chalf *c, const chalf *a, const chalf *b,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));
inline void matmul_4xVL(chalf *c, const chalf *a, const chalf *b,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

#endif