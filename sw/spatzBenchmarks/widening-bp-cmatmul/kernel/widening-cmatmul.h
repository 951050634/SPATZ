#ifndef WIDENINGFMATMUL_H
#define WIDENINGFMATMUL_H

typedef struct
{
    char real;
    char imag;
} cbyte;

void matmul(cbyte *c, const cbyte *a, const cbyte *b, const unsigned int M,
            const unsigned int N, const unsigned int P);

inline void matmul_2xVL(cbyte *c, const cbyte *a, const cbyte *b,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));
inline void matmul_4xVL(cbyte *c, const cbyte *a, const cbyte *b,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

#endif
