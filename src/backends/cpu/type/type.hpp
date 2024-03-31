//
// Created by shrelic on 24-3-5.
//

#ifndef MLLM_TYPE_HPP
#define MLLM_TYPE_HPP

#define MLLM_RESTRICT __restrict
#include "Types.hpp"

typedef void (*mllm_to_float_func)(const void *src, void *dst, const int n); // from src type to float(stored in dst)  n is the number of element in src
typedef void (*mllm_from_float_func)(const float *src, void  *dst, const int n);
typedef void (*mllm_vec_dot_func)   (const int n, float * MLLM_RESTRICT dst, const void * MLLM_RESTRICT x, const void * MLLM_RESTRICT y);
typedef void (*mllm_vec_add_row_func) (const int n, const void * MLLM_RESTRICT src, float * MLLM_RESTRICT dst, const float alpha);

typedef struct type_traits_t{
    size_t size;  // type size
    int blck_size; // number of element in a block (quantization block)
    mllm_to_float_func to_float;
    mllm_from_float_func from_float;
    mllm_vec_dot_func vec_dot;
    DataType vec_dot_type; // vec_dot do dot product between two DataType, this is the other type
    mllm_vec_add_row_func add_row_to; // add alpha * row to a row of float
}type_traits_t;

extern type_traits_t type_traits[];

inline size_t type_size(DataType type){
    return type_traits[type].size;
}

inline int blck_size(DataType type){
    return type_traits[type].blck_size;
}

// ne: number of elements in a row
// return the number of bytes in a row
inline size_t row_size(DataType type, int64_t ne) {
    return type_size(type) * ne / blck_size(type);
}

#endif // MLLM_TYPE_HPP
