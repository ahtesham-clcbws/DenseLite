#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "model_manager.hpp"
#include "resource_governor.hpp"

enum class MultimodalMode {
    PROCEDURAL_ACOUSTIC_FALLBACK,
    LEASED_NEURAL_WHISPER,
    PROCEDURAL_LATENT_FALLBACK,
    LEASED_NEURAL_DIFFUSION
};

struct TranscribeResult {
    bool success = false;
    std::string text;
    std::string detected_language = "en";
    float duration_seconds = 0.0f;
    std::string engine_mode = "acoustic_spectral_fourier";
    MultimodalMode mode = MultimodalMode::PROCEDURAL_ACOUSTIC_FALLBACK;
    std::string error_message;
};

struct ImageGenerationResult {
    bool success = false;
    std::string prompt;
    int width = 512;
    int height = 512;
    int steps = 20;
    int seed = 42;
    std::string format = "png";
    std::string engine_mode = "latent_vae_synthesis";
    MultimodalMode mode = MultimodalMode::PROCEDURAL_LATENT_FALLBACK;
    size_t data_bytes = 0;
    std::vector<uint8_t> rgba_data;
    std::string error_message;
};

class MultimodalEngine {
public:
    explicit MultimodalEngine(ModelManager* model_manager = nullptr, ResourceGovernor* governor = nullptr);

    // Audio / Speech-to-Text via Whisper (on-demand leased lifecycle)
    TranscribeResult transcribe_audio(const std::vector<float>& pcm_audio, int sample_rate = 16000);
    TranscribeResult transcribe_pcm_bytes(const std::vector<uint8_t>& audio_bytes);

    // Image generation via Stable Diffusion (on-demand leased lifecycle)
    ImageGenerationResult generate_image(
        const std::string& prompt,
        int width = 512,
        int height = 512,
        int steps = 20,
        int seed = 42);

    // Capability check under current hardware resource constraints
    bool can_generate_image(int width, int height) const;
    bool can_transcribe_audio(float estimated_seconds) const;

private:
    ModelManager* model_manager_ = nullptr;
    ResourceGovernor* governor_ = nullptr;
};
