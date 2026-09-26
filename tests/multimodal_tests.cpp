#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include "multimodal_engine.hpp"
#include "RequestAnalyzer.hpp"

void test_audio_transcription() {
    MultimodalEngine engine;

    // 1. Empty audio buffer handled safely
    auto empty_res = engine.transcribe_audio({});
    assert(!empty_res.success);

    // 2. Synthesize 1-second 440Hz sine tone (16kHz sample rate)
    std::vector<float> sine_wave(16000);
    for (size_t i = 0; i < sine_wave.size(); ++i) {
        sine_wave[i] = 0.5f * std::sin(2.0f * 3.14159f * 440.0f * i / 16000.0f);
    }

    auto audio_res = engine.transcribe_audio(sine_wave, 16000);
    assert(audio_res.success);
    assert(audio_res.duration_seconds == 1.0f);
    assert(!audio_res.text.empty());
    assert(audio_res.detected_language == "en");

    // 3. Silence test
    std::vector<float> silence(16000, 0.0f);
    auto silence_res = engine.transcribe_audio(silence, 16000);
    assert(silence_res.success);
    assert(silence_res.text.find("silence") != std::string::npos);

    std::cout << "[PASS] test_audio_transcription\n";
}

void test_pcm_byte_decoding() {
    MultimodalEngine engine;

    // Too short byte stream
    auto short_res = engine.transcribe_pcm_bytes({0x00});
    assert(!short_res.success);

    // Valid 16-bit PCM bytes (500 samples = 1000 bytes)
    std::vector<uint8_t> pcm_bytes(1000, 0x10);
    auto pcm_res = engine.transcribe_pcm_bytes(pcm_bytes);
    assert(pcm_res.success);
    assert(pcm_res.duration_seconds > 0.0f);

    std::cout << "[PASS] test_pcm_byte_decoding\n";
}

void test_image_generation() {
    MultimodalEngine engine;

    // 1. Empty prompt rejected
    auto empty_res = engine.generate_image("");
    assert(!empty_res.success);

    // 2. Normal generation
    auto img_res = engine.generate_image("A futuristic city in cyberpunk neon", 512, 512, 10, 1234);
    assert(img_res.success);
    assert(img_res.width == 512);
    assert(img_res.height == 512);
    assert(img_res.steps == 10);
    assert(img_res.format == "png");
    assert(img_res.data_bytes == 512 * 512 * 4);

    std::cout << "[PASS] test_image_generation\n";
}

void test_request_analyzer_multimodal_classification() {
    OpenAIRequest audio_req;
    audio_req.messages.push_back({"user", "Please transcribe this audio recording of the team meeting", "", ""});
    assert(RequestAnalyzer::categorize_request(audio_req) == "audio");

    OpenAIRequest image_req;
    image_req.messages.push_back({"user", "Generate an image of a majestic eagle over snow mountains", "", ""});
    assert(RequestAnalyzer::categorize_request(image_req) == "image");

    OpenAIRequest code_req;
    code_req.messages.push_back({"user", "Implement a binary search tree in C++", "", ""});
    assert(RequestAnalyzer::categorize_request(code_req) == "coding");

    std::cout << "[PASS] test_request_analyzer_multimodal_classification\n";
}

int main() {
    std::cout << "--- Running Phase 8 Multimodal Vision & Speech Tests ---\n";
    test_audio_transcription();
    test_pcm_byte_decoding();
    test_image_generation();
    test_request_analyzer_multimodal_classification();
    std::cout << "--- All Phase 8 Multimodal Tests Passed! ---\n";
    return 0;
}
