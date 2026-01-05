#ifndef _CAXPY_H_
#define _CAXPY_H_

inline void caxpy_v64b(const cdouble a, const cdouble *x, const cdouble *y,
                       unsigned int avl) __attribute__((always_inline));

inline void caxpy_v32b(const cfloat a, const cfloat *x, cfloat *y, 
                       unsigned int avl) __attribute__((always_inline));

inline void caxpy_v16b(const chalf a, const chalf *x, chalf *y,
                       unsigned int avl) __attribute__((always_inline));

#endif