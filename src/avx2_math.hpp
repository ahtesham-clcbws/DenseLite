#pragma once

#include "model.hpp"
#include <immintrin.h>
#include <cmath>
#include <cstdint>
#include <cassert>

// ============================================================================
// Zero-BLAS AVX2 Math Kernels for DenseLite
// ============================================================================

namespace math {

// ----------------------------------------------------------------------------
// 1. Q8_0 x FP32 Dot Product (Core MatMul Kernel)
// ----------------------------------------------------------------------------
// Dequantizes a 34-byte Q8_0 block on the fly and computes the dot product
// against 32 FP32 activation values.
inline float dot_product_q8_fp32(const block_q8_0* __restrict x, const float* __restrict y, int nb) {
    // nb is the number of blocks (each block is 32 elements).
    
    // Accumulator register initialized to zero
    __m256 acc = _mm256_setzero_ps();
    
    for (int i = 0; i < nb; ++i) {
        // Step 1: Extract the FP16 scale 'd' and convert to FP32.
        // We use an intrinsic to convert a single FP16 value to FP32.
        __m128i d_16 = _mm_set1_epi16(x[i].d);
        __m128 d_32 = _mm_cvtph_ps(d_16);
        // Broadcast the scale factor across all 8 lanes of a 256-bit register
        __m256 vd = _mm256_broadcastss_ps(d_32);
        
        // Step 2: Load 32 quantized 8-bit integers from the block
        __m256i vq0 = _mm256_loadu_si256((const __m256i*) &x[i].qs[0]);
        
        // Step 3: Expand the 8-bit ints into 32-bit integers in 4 chunks of 8
        // because an AVX2 256-bit register can only hold 8x 32-bit values.
        
        // Lower 128 bits (first 16 ints)
        __m128i vq_lo = _mm256_castsi256_si128(vq0);
        // Upper 128 bits (next 16 ints)
        __m128i vq_hi = _mm256_extracti128_si256(vq0, 1);
        
        // Expand to 32-bit integers
        __m256i vi0 = _mm256_cvtepi8_epi32(vq_lo);                     // [0..7]
        __m256i vi1 = _mm256_cvtepi8_epi32(_mm_srli_si128(vq_lo, 8)); // [8..15]
        __m256i vi2 = _mm256_cvtepi8_epi32(vq_hi);                     // [16..23]
        __m256i vi3 = _mm256_cvtepi8_epi32(_mm_srli_si128(vq_hi, 8)); // [24..31]
        
        // Convert 32-bit ints to 32-bit floats
        __m256 vf0 = _mm256_cvtepi32_ps(vi0);
        __m256 vf1 = _mm256_cvtepi32_ps(vi1);
        __m256 vf2 = _mm256_cvtepi32_ps(vi2);
        __m256 vf3 = _mm256_cvtepi32_ps(vi3);
        
        // Multiply by the scale factor 'vd' to dequantize
        vf0 = _mm256_mul_ps(vf0, vd);
        vf1 = _mm256_mul_ps(vf1, vd);
        vf2 = _mm256_mul_ps(vf2, vd);
        vf3 = _mm256_mul_ps(vf3, vd);
        
        // Step 4: Load 32 FP32 values from the activation vector 'y'
        // (Assuming 'y' is aligned to 32 bytes)
        __m256 vy0 = _mm256_loadu_ps(&y[i * 32 + 0]);
        __m256 vy1 = _mm256_loadu_ps(&y[i * 32 + 8]);
        __m256 vy2 = _mm256_loadu_ps(&y[i * 32 + 16]);
        __m256 vy3 = _mm256_loadu_ps(&y[i * 32 + 24]);
        
        // Step 5: FMA (Fused Multiply-Add) to accumulate the dot product
        acc = _mm256_fmadd_ps(vf0, vy0, acc);
        acc = _mm256_fmadd_ps(vf1, vy1, acc);
        acc = _mm256_fmadd_ps(vf2, vy2, acc);
        acc = _mm256_fmadd_ps(vf3, vy3, acc);
    }
    
    // Step 6: Horizontal sum of the 8 floats in the accumulator
    __m128 acc_hi = _mm256_extractf128_ps(acc, 1);
    __m128 acc_lo = _mm256_castps256_ps128(acc);
    __m128 sum128 = _mm_add_ps(acc_hi, acc_lo);
    
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    
    return _mm_cvtss_f32(sum128);
}

// ----------------------------------------------------------------------------
// 2. RMSNorm (SIMD Optimized)
// ----------------------------------------------------------------------------
inline void rmsnorm(float* __restrict out, const float* __restrict x, int size, float eps, const float* __restrict weight) {
    // Note: Assuming 'size' is a multiple of 8 for pure AVX2.
    assert(size % 8 == 0);
    
    __m256 sum_sq = _mm256_setzero_ps();
    for (int i = 0; i < size; i += 8) {
        __m256 v = _mm256_loadu_ps(&x[i]);
        sum_sq = _mm256_fmadd_ps(v, v, sum_sq);
    }
    
    // Horizontal sum
    __m128 hi = _mm256_extractf128_ps(sum_sq, 1);
    __m128 lo = _mm256_castps256_ps128(sum_sq);
    __m128 sum = _mm_add_ps(hi, lo);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    float ss = _mm_cvtss_f32(sum);
    
    float scale = 1.0f / std::sqrt(ss / size + eps);
    __m256 vscale = _mm256_set1_ps(scale);
    
    for (int i = 0; i < size; i += 8) {
        __m256 v = _mm256_loadu_ps(&x[i]);
        __m256 w = _mm256_loadu_ps(&weight[i]); // RMSNorm scaling weights
        __m256 res = _mm256_mul_ps(_mm256_mul_ps(v, vscale), w);
        _mm256_storeu_ps(&out[i], res);
    }
}

// ----------------------------------------------------------------------------
// 3. Softmax (Standard Implementation)
// ----------------------------------------------------------------------------
inline void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; ++i) {
        if (x[i] > max_val) max_val = x[i];
    }
    
    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        x[i] = std::exp(x[i] - max_val);
        sum += x[i];
    }
    
    for (int i = 0; i < size; ++i) {
        x[i] /= sum;
    }
}

// ----------------------------------------------------------------------------
// 4. Rotary Positional Embedding (RoPE)
// ----------------------------------------------------------------------------
inline void rope(float* vec, int pos, int num_heads, int head_dim, const float* inv_freq) {
    // Standard RoPE implementation
    for (int h = 0; h < num_heads; ++h) {
        float* head = vec + h * head_dim;
        for (int i = 0; i < head_dim; i += 2) {
            float theta = (float)pos * inv_freq[i / 2];
            float cos_theta = std::cos(theta);
            float sin_theta = std::sin(theta);
            
            float x0 = head[i];
            float x1 = head[i + 1];
            head[i]     = x0 * cos_theta - x1 * sin_theta;
            head[i + 1] = x0 * sin_theta + x1 * cos_theta;
        }
    }
}

// ----------------------------------------------------------------------------
// 5. SwiGLU Activation (SiLU * gate)
// ----------------------------------------------------------------------------
inline void swiglu(float* out_and_gate, const float* up, int size) {
    // Note: The caller passes the gate tensor to `out_and_gate`.
    // The operation is performed in-place. Because it computes element-by-element,
    // reading from and writing to `out_and_gate` simultaneously is safe.
    // out[i] = up[i] * (gate[i] * sigmoid(gate[i]))
    for (int i = 0; i < size; ++i) {
        float silu = out_and_gate[i] / (1.0f + std::exp(-out_and_gate[i]));
        out_and_gate[i] = up[i] * silu;
    }
}

} // namespace math
