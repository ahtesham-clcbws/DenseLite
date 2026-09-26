#include "../src/avx2_math.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <cassert>

// Scalar reference implementations

inline float fp16_to_fp32_scalar(uint16_t h) {
    __m128i h_vec = _mm_set1_epi16(h);
    __m128 f_vec = _mm_cvtph_ps(h_vec);
    return _mm_cvtss_f32(f_vec);
}

float scalar_dot_product_q8_fp32(const block_q8_0* x, const float* y, int nb) {
    float sum = 0.0f;
    for (int i = 0; i < nb; ++i) {
        float d = fp16_to_fp32_scalar(x[i].d);
        for (int j = 0; j < 32; ++j) {
            float dequant = x[i].qs[j] * d;
            sum += dequant * y[i * 32 + j];
        }
    }
    return sum;
}

void scalar_rmsnorm(float* out, const float* x, int size, float eps, const float* weight) {
    float sum_sq = 0.0f;
    for (int i = 0; i < size; ++i) {
        sum_sq += x[i] * x[i];
    }
    float scale = 1.0f / std::sqrt(sum_sq / size + eps);
    for (int i = 0; i < size; ++i) {
        out[i] = x[i] * scale * weight[i];
    }
}

void scalar_rope(float* vec, int pos, int num_heads, int head_dim, const float* inv_freq) {
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

void scalar_swiglu(float* out_and_gate, const float* up, int size) {
    for (int i = 0; i < size; ++i) {
        float silu = out_and_gate[i] / (1.0f + std::exp(-out_and_gate[i]));
        out_and_gate[i] = up[i] * silu;
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "[TEST] Phase 1: Math Correctness (AVX2 vs Scalar)\n";
    std::cout << "========================================\n";

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> fdist(-2.0f, 2.0f);
    std::uniform_int_distribution<int> idist(-127, 127);

    // 1. Test dot_product_q8_fp32
    {
        constexpr int nb = 32; // 32 * 32 = 1024 floats
        std::vector<block_q8_0> blocks(nb);
        std::vector<float> y(nb * 32);
        
        // Random FP16 scaling factor d
        uint16_t d_fp16 = 0x3C00; // 1.0 in FP16
        for (int i = 0; i < nb; ++i) {
            blocks[i].d = d_fp16;
            for (int j = 0; j < 32; ++j) {
                blocks[i].qs[j] = static_cast<int8_t>(idist(rng));
                y[i * 32 + j] = fdist(rng);
            }
        }

        float avx2_res = math::dot_product_q8_fp32(blocks.data(), y.data(), nb);
        float scalar_res = scalar_dot_product_q8_fp32(blocks.data(), y.data(), nb);
        float diff = std::abs(avx2_res - scalar_res);
        float rel_err = diff / std::max(1.0f, std::abs(scalar_res));

        std::cout << "1. dot_product_q8_fp32: diff = " << diff << ", rel_err = " << rel_err 
                  << " (AVX2=" << avx2_res << ", Scalar=" << scalar_res << ")\n";
        if (rel_err > 1e-5) {
            std::cerr << "FAIL: dot_product_q8_fp32 relative error exceeds 1e-5!\n";
            return 1;
        }
        std::cout << "   -> PASS (rel_err < 1e-5)\n";
    }

    // 2. Test rmsnorm
    {
        constexpr int size = 1536; // Qwen embedding size
        std::vector<float> x(size);
        std::vector<float> weight(size);
        std::vector<float> out_avx2(size);
        std::vector<float> out_scalar(size);

        for (int i = 0; i < size; ++i) {
            x[i] = fdist(rng);
            weight[i] = fdist(rng);
        }

        math::rmsnorm(out_avx2.data(), x.data(), size, 1e-6f, weight.data());
        scalar_rmsnorm(out_scalar.data(), x.data(), size, 1e-6f, weight.data());

        float max_diff = 0.0f;
        for (int i = 0; i < size; ++i) {
            float d = std::abs(out_avx2[i] - out_scalar[i]);
            if (d > max_diff) max_diff = d;
        }
        std::cout << "2. rmsnorm: max_diff = " << max_diff << "\n";
        if (max_diff > 1e-5) {
            std::cerr << "FAIL: rmsnorm exceeds tolerance 1e-5!\n";
            return 1;
        }
        std::cout << "   -> PASS (|max_diff| < 1e-5)\n";
    }

    // 3. Test rope
    {
        constexpr int num_heads = 12;
        constexpr int head_dim = 128;
        constexpr int total_dim = num_heads * head_dim;
        std::vector<float> vec_avx2(total_dim);
        std::vector<float> vec_scalar(total_dim);
        std::vector<float> inv_freq(head_dim / 2);

        float base = 1000000.0f;
        for (int i = 0; i < head_dim; i += 2) {
            inv_freq[i / 2] = 1.0f / std::pow(base, (float)i / head_dim);
        }

        for (int i = 0; i < total_dim; ++i) {
            float val = fdist(rng);
            vec_avx2[i] = val;
            vec_scalar[i] = val;
        }

        int pos = 42;
        math::rope(vec_avx2.data(), pos, num_heads, head_dim, inv_freq.data());
        scalar_rope(vec_scalar.data(), pos, num_heads, head_dim, inv_freq.data());

        float max_diff = 0.0f;
        for (int i = 0; i < total_dim; ++i) {
            float d = std::abs(vec_avx2[i] - vec_scalar[i]);
            if (d > max_diff) max_diff = d;
        }
        std::cout << "3. rope: max_diff = " << max_diff << "\n";
        if (max_diff > 1e-5) {
            std::cerr << "FAIL: rope exceeds tolerance 1e-5!\n";
            return 1;
        }
        std::cout << "   -> PASS (|max_diff| < 1e-5)\n";
    }

    // 4. Test swiglu
    {
        constexpr int size = 8960; // Qwen intermediate size
        std::vector<float> gate_avx2(size);
        std::vector<float> gate_scalar(size);
        std::vector<float> up(size);

        for (int i = 0; i < size; ++i) {
            float g = fdist(rng);
            gate_avx2[i] = g;
            gate_scalar[i] = g;
            up[i] = fdist(rng);
        }

        math::swiglu(gate_avx2.data(), up.data(), size);
        scalar_swiglu(gate_scalar.data(), up.data(), size);

        float max_diff = 0.0f;
        for (int i = 0; i < size; ++i) {
            float d = std::abs(gate_avx2[i] - gate_scalar[i]);
            if (d > max_diff) max_diff = d;
        }
        std::cout << "4. swiglu: max_diff = " << max_diff << "\n";
        if (max_diff > 1e-5) {
            std::cerr << "FAIL: swiglu exceeds tolerance 1e-5!\n";
            return 1;
        }
        std::cout << "   -> PASS (|max_diff| < 1e-5)\n";
    }

    std::cout << "\n>>> ALL MATH CORRECTNESS TESTS PASSED! <<<\n";
    return 0;
}
