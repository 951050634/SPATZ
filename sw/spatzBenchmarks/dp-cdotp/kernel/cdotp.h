#ifndef _FDOTPROD_H_
#define _FDOTPROD_H_

inline cdouble cdotp_v64b(const cdouble *x, const cdouble *y, unsigned int avl)
    __attribute__((always_inline));
inline cfloat cdotp_v32b(const cfloat *x, const cfloat *y, unsigned int avl)
    __attribute__((always_inline));
inline chalf cdotp_v16b(const chalf *x, const chalf *y, unsigned int avl)
    __attribute__((always_inline));

#endif