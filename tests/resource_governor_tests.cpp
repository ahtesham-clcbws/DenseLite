#ifdef NDEBUG
#undef NDEBUG
#endif
#include <iostream>
#include <cassert>
#include <omp.h>
#include "resource_governor.hpp"
#include "ResourcePolicy.hpp"
#include "model.hpp"

void test_system_queries() {
    assert(ResourcePolicy::thread_limit_for_hardware(4) == 2);
    assert(ResourcePolicy::thread_limit_for_hardware(4, 4) == 2);
    assert(ResourcePolicy::thread_limit_for_hardware(2) == 1);
    assert(ResourcePolicy::thread_limit_for_hardware(1) == 1);
    assert(ResourcePolicy::compute_safe_ram_ceiling(1.0f) == ResourcePolicy::compute_safe_ram_ceiling());
    assert(ResourceGovernor::get_process_rss_bytes() > 0);
    assert(native_tensor_supported("blk.0.attn_norm.weight", TensorType::FP32));
    assert(!native_tensor_supported("blk.0.attn_norm.weight", TensorType::FP16));
    assert(native_tensor_supported("token_embd.weight", TensorType::Q8_0));
    assert(!native_tensor_supported("token_embd.weight", TensorType::Q4_K));
    assert(tensor_type_name(TensorType::Q5_1) == "Q5_1");
    assert(ResourceGovernor::get_host_total_ram_bytes() > 0);
    assert(ResourceGovernor::get_host_available_ram_bytes() > 0);
    assert(ResourceGovernor::get_host_available_ram_bytes() <= ResourceGovernor::get_host_total_ram_bytes());
    int expected_max = std::max(1u, std::thread::hardware_concurrency() / 2);
    assert(ResourceGovernor::get_max_allowed_threads() >= 1 && ResourceGovernor::get_max_allowed_threads() <= expected_max);
    std::cout << "[PASS] test_system_queries\n";
}

void test_thread_enforcement() {
    ResourceGovernor::enforce_thread_limits();
    int threads_observed = 0;
    #pragma omp parallel
    {
        #pragma omp single
        {
            threads_observed = omp_get_num_threads();
        }
    }
    assert(threads_observed <= ResourceGovernor::get_max_allowed_threads());
    std::cout << "[PASS] test_thread_enforcement (threads=" << threads_observed << ")\n";
}

void test_component_memory_tracking() {
    ResourceGovernor gov;
    gov.track_inference_memory(50 * 1024 * 1024);
    gov.track_kv_cache(224 * 1024 * 1024);
    gov.track_vector_store(30 * 1024 * 1024);
    gov.track_model_weights(1200 * 1024 * 1024);

    assert(gov.get_total_tracked_bytes() == ((50ULL + 224 + 30 + 1200) * 1024 * 1024));

    auto snap = gov.get_snapshot();
    assert(snap.inference_memory_bytes == 50 * 1024 * 1024);
    assert(snap.kv_cache_bytes == 224 * 1024 * 1024);
    assert(snap.vector_store_bytes == 30 * 1024 * 1024);
    assert(snap.model_weights_bytes == 1200 * 1024 * 1024);
    assert(snap.max_threads == ResourceGovernor::get_max_allowed_threads());
    (void)snap;
    std::cout << "[PASS] test_component_memory_tracking\n";
}

void test_eviction_stages_and_headroom() {
    ResourceGovernor gov;
    EvictionStage stage = gov.assess_eviction_stage();
    assert(stage >= EvictionStage::NONE && stage <= EvictionStage::ROUTE_CLOUD);

    assert(gov.should_route_to_cloud() == (stage >= EvictionStage::ROUTE_CLOUD));
    assert(gov.should_reject_optional_load() == (stage >= EvictionStage::REJECT_OPTIONAL));
    (void)stage;

    // Requesting impossible amount of RAM should fail admission
    assert(!gov.can_admit_host_ram(100ULL * 1024 * 1024 * 1024));
    assert(!gov.can_admit_host_ram(static_cast<size_t>(-1)));

    gov.track_model_weights(ResourcePolicy::compute_safe_ram_ceiling());
    assert(!gov.can_admit_host_ram(1));
    gov.track_model_weights(0);

    // Requesting small amount should succeed if system has memory
    if (ResourceGovernor::get_host_available_ram_bytes() > 2ULL * 1024 * 1024 * 1024) {
        assert(gov.can_admit_host_ram(1024 * 1024));
    }

    std::cout << "[PASS] test_eviction_stages_and_headroom\n";
}

void test_dynamic_context_sizing() {
    size_t ctx = ResourceGovernor::calculate_dynamic_context_tokens();
    assert(ctx == 65536 || ctx == 32768 || ctx == 16384 || (ctx <= 8192 && ctx % 1024 == 0));
    std::cout << "[PASS] test_dynamic_context_sizing (allocated_tokens=" << ctx << ")\n";
}

int main() {
    std::cout << "--- Running Phase 7 Resource Governance Tests ---\n";
    test_system_queries();
    test_thread_enforcement();
    test_component_memory_tracking();
    test_eviction_stages_and_headroom();
    test_dynamic_context_sizing();
    std::cout << "--- All Phase 7 Resource Governance Tests Passed! ---\n";
    return 0;
}

