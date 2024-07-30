#ifndef MLLM_GEMM_AARCH64_HPP
#define MLLM_GEMM_AARCH64_HPP


#include "VecDot.hpp"
using namespace mllm;



// Quantization
void quantize_q8_0_4x4(const float * __restrict x, void * __restrict y, int64_t k);
void quantize_q8_0_4x8(const float * __restrict x, void * __restrict y, int64_t k);

void quantize_mat_q8_0(const float * __restrict x, void * __restrict y, int64_t nrows, int64_t n_per_row, int64_t blck_size_interleave);

// Quantization utilizing an importance matrix (a.k.a. "Activation aWare Quantization")
size_t quantize_q4_0_4x4(const float * __restrict src, void * __restrict dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
size_t quantize_q4_0_4x8(const float * __restrict src, void * __restrict dst, int64_t nrows, int64_t n_per_row, const float * imatrix);
size_t quantize_q4_0_8x8(const float * __restrict src, void * __restrict dst, int64_t nrows, int64_t n_per_row, const float * imatrix);

// GEMV
void mllm_gemv_q4_0_4x4_q8_0(int n, float * __restrict s, size_t bs, const void * __restrict vx, const void * __restrict vy, int nr, int nc);
void mllm_gemv_q4_0_4x8_q8_0(int n, float * __restrict s, size_t bs, const void * __restrict vx, const void * __restrict vy, int nr, int nc);
void mllm_gemv_q4_0_8x8_q8_0(int n, float * __restrict s, size_t bs, const void * __restrict vx, const void * __restrict vy, int nr, int nc);

// GEMM
void mllm_gemm_q4_0_4x4_q8_0(int n, float * __restrict s, size_t bs, const void * __restrict vx, const void * __restrict vy, int nr, int nc);
void mllm_gemm_q4_0_4x8_q8_0(int n, float * __restrict s, size_t bs, const void * __restrict vx, const void * __restrict vy, int nr, int nc);
void mllm_gemm_q4_0_8x8_q8_0(int n, float * __restrict s, size_t bs, const void * __restrict vx, const void * __restrict vy, int nr, int nc);

void quantize_row_q4_0_4x4(const float * __restrict x, void * __restrict y, int k);
void quantize_row_q4_0_4x4(const float * __restrict x, void * __restrict y, int k, int raw);

#endif // MLLM_GEMM_HPP