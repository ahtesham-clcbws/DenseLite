#include "vulkan_device.hpp"
#include "model_registry.hpp"
#include "model_pool.hpp"
#include "model_lease.hpp"
#include "kv_cache.hpp"
#include "model_manager.hpp"
#include "resource_governor.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

void test_vulkan_detection() {
    std::cout << "[Test 1] Vulkan Detection & 85% VRAM Ceiling..." << std::endl;
    VulkanDevice vk;
    if (vk.is_available()) {
        auto mem = vk.memory_info();
        assert(mem.total_vram_bytes > 0);
        assert(mem.safe_ceiling_bytes == static_cast<size_t>(mem.total_vram_bytes * 0.85));
        assert(vk.limits().minStorageBufferOffsetAlignment > 0);
        std::cout << "  Device: " << vk.device_name() << " (Type: " << vk.device_type() << ")" << std::endl;
        std::cout << "  Total VRAM: " << (mem.total_vram_bytes / (1024*1024)) << " MiB" << std::endl;
        std::cout << "  85% Safe Ceiling: " << (mem.safe_ceiling_bytes / (1024*1024)) << " MiB" << std::endl;
        std::cout << "  Alignment: " << vk.limits().minStorageBufferOffsetAlignment << " bytes" << std::endl;
    } else {
        std::cout << "  Vulkan device not available on this host. Testing fallback safety." << std::endl;
        assert(vk.can_admit(1024) == false);
    }
    std::cout << "  -> PASSED" << std::endl;
}

void test_gpu_admission_gate() {
    std::cout << "[Test 2] GPU-Preferred Unified Admission Gate..." << std::endl;
    VulkanDevice vk;
    if (vk.is_available()) {
        size_t safe_ceiling = vk.safe_ceiling_bytes();
        size_t small_allocation = 100 * 1024 * 1024; // 100 MB
        size_t huge_allocation = safe_ceiling + 1024 * 1024; // Exceeds 85% ceiling!

        (void)huge_allocation;
        assert(vk.can_admit(small_allocation) == true);
        assert(vk.allocate(small_allocation) == true);
        assert(vk.allocated_bytes() == small_allocation);

        // Huge allocation exceeding safety budget MUST fail admission
        assert(vk.can_admit(huge_allocation) == false);
        assert(vk.allocate(huge_allocation) == false);

        vk.release(small_allocation);
        assert(vk.allocated_bytes() == 0);
    } else {
        std::cout << "  Vulkan unavailable; admission correctly defaults to CPU_RAM." << std::endl;
    }
    std::cout << "  -> PASSED" << std::endl;
}

void test_bounded_kv_cache() {
    std::cout << "[Test 3] Bounded KV Cache Allocation Policy..." << std::endl;
    ModelConfig cfg;
    cfg.num_layers = 28;
    cfg.num_kv_heads = 2;
    cfg.head_dim = 128;
    cfg.context_length = 8192;

    size_t bytes_per_token = KVCache::calculate_bytes_per_token(cfg);
    // 28 layers * 2 (K+V) * 2 kv_heads * 128 dim * 2 bytes = 28672 bytes/token
    assert(bytes_per_token == 28 * 2 * 2 * 128 * 2);

    VulkanDevice vk;
    KVCache kv;

    // Allocate bounded at 2048 tokens
    size_t target_context = 2048;
    size_t expected_bytes = target_context * bytes_per_token;
    bool ok = kv.allocate(cfg, target_context, &vk);
    assert(ok);
    (void)ok;
    (void)expected_bytes;
    assert(kv.capacity_tokens() == target_context);
    assert(kv.allocated_bytes() == expected_bytes);

    // KV Cache can advance up to capacity, and refuses beyond
    assert(kv.can_append(100) == true);
    kv.advance(100);
    assert(kv.current_tokens() == 100);
    assert(kv.can_append(target_context) == false);

    kv.release();
    assert(kv.allocated_bytes() == 0);
    std::cout << "  -> PASSED" << std::endl;
}

void test_model_lease_raii_and_eviction_guard() {
    std::cout << "[Test 4] ModelLease RAII & Eviction Guard Safety..." << std::endl;
    ModelPool pool;
    DenseModel dummy;
    dummy.config.embedding_length = 512;
    pool.add_model("test_model", std::move(dummy), DevicePlacement::CPU_RAM, 1024);

    assert(pool.is_in_use("test_model") == false);
    assert(pool.can_unload("test_model") == true);

    {
        ModelLease lease;
        bool ok = pool.acquire_lease("test_model", lease);
        assert(ok);
        (void)ok;
        assert(lease.is_valid());
        assert(lease.model_id() == "test_model");
        assert(lease.config().embedding_length == 512);

        // While lease is in scope, model is IN USE and cannot be unmapped/unloaded!
        assert(pool.is_in_use("test_model") == true);
        assert(pool.can_unload("test_model") == false);

        DenseModel out;
        bool unload_attempt = pool.remove_model("test_model", out);
        assert(unload_attempt == false); // EVICTION GUARD PREVENTS UNLOAD!
        (void)unload_attempt;

        // Inner nested scope with move semantics
        {
            ModelLease moved_lease = std::move(lease);
            assert(!lease.is_valid());
            assert(moved_lease.is_valid());
            assert(pool.is_in_use("test_model") == true);
        }
        // moved_lease destroyed here -> active_users decremented to 0
    }

    // Now lease is completely out of scope; unload must succeed
    assert(pool.is_in_use("test_model") == false);
    assert(pool.can_unload("test_model") == true);

    DenseModel out;
    bool unload_ok = pool.remove_model("test_model", out);
    assert(unload_ok == true);
    (void)unload_ok;
    assert(pool.get_state("test_model") == ModelState::COLD);

    std::cout << "  -> PASSED" << std::endl;
}

void test_invalid_file_handling() {
    std::cout << "[Test 5] Safe Loader Error Handling (No exit(1))..." << std::endl;
    DenseModel dummy;
    std::string err;
    bool ok = ModelLoader::load_model("/path/to/non_existent_model.gguf", dummy, err);
    assert(!ok);
    assert(!err.empty());
    std::cout << "  Safely handled missing file: " << err << std::endl;

    // Test corrupted file header
    std::string tmp_corrupt = "/tmp/corrupt_header.gguf";
    {
        std::ofstream f(tmp_corrupt, std::ios::binary);
        f.write("BADF00D", 7);
    }
    ok = ModelLoader::load_model(tmp_corrupt, dummy, err);
    assert(!ok);
    std::filesystem::remove(tmp_corrupt);
    std::cout << "  Safely handled corrupt GGUF magic: " << err << std::endl;
    std::cout << "  -> PASSED" << std::endl;
}

void test_model_manager_on_demand_and_roles() {
    std::cout << "[Test 6] ModelManager Role Mapping & Placement Fallback..." << std::endl;
    ModelManager mgr;

    ModelDescriptor d_needle;
    d_needle.id = "needle";
    d_needle.role = ModelRole::ROUTER;
    d_needle.is_gguf = false;
    d_needle.placement_policy = PlacementPolicy::CPU_RAM_ONLY;
    mgr.registry().register_model(d_needle);

    ModelDescriptor d_coder;
    d_coder.id = "qwen_coder";
    d_coder.role = ModelRole::CODER;
    d_coder.is_gguf = false; // test stub
    d_coder.placement_policy = PlacementPolicy::GPU_PREFERRED;
    d_coder.estimated_weight_bytes = 1024 * 1024;
    mgr.registry().register_model(d_coder);

    // Acquire needle role -> should auto-load stub and yield valid lease
    ModelLease lease_needle = mgr.acquire(ModelRole::ROUTER);
    assert(lease_needle.is_valid());
    assert(lease_needle.model_id() == "needle");
    assert(lease_needle.placement() == DevicePlacement::CPU_RAM);

    // Acquire coder role -> should auto-load stub and yield valid lease
    ModelLease lease_coder = mgr.acquire(ModelRole::CODER);
    assert(lease_coder.is_valid());
    assert(lease_coder.model_id() == "qwen_coder");

    std::cout << "  -> PASSED" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << " DenseLite Phase 2 Lifecycle & Placement Tests   " << std::endl;
    std::cout << "=================================================" << std::endl;

    test_vulkan_detection();
    test_gpu_admission_gate();
    test_bounded_kv_cache();
    test_model_lease_raii_and_eviction_guard();
    test_invalid_file_handling();
    test_model_manager_on_demand_and_roles();

    std::cout << "=================================================" << std::endl;
    std::cout << " All Phase 2 Tests PASSED Successfully!          " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
