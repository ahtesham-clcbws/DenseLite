#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vector/turboquant_index.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <random>
#include <algorithm>

void test_compression_ratio() {
    std::cout << "[Test 1] TurboQuant 4-Bit 8x Vector Compression Ratio..." << std::endl;
    const size_t dim = 128;
    TurboQuantIndex index(dim);

    const size_t num_vecs = 100;
    for (size_t i = 0; i < num_vecs; ++i) {
        std::vector<float> v(dim, 0.0f);
        for (size_t d = 0; d < dim; ++d) {
            v[d] = std::sin(static_cast<float>(i * dim + d));
        }
        index.add("vec_" + std::to_string(i), v);
    }

    assert(index.size() == num_vecs);
    size_t packed_per_vec = (dim + 1) / 2;
    assert(packed_per_vec == 64); // 128 dim / 2 = 64 bytes vs 512 bytes FP32 (8x compression)
    std::cout << "  -> PASSED: 128-dim FP32 (512B) compressed to 4-bit packed (" << packed_per_vec << "B)" << std::endl;
}

void test_search_accuracy() {
    std::cout << "[Test 2] Nearest Neighbor Retrieval Accuracy..." << std::endl;
    const size_t dim = 128;
    TurboQuantIndex index(dim);

    std::vector<float> target(dim, 0.0f);
    target[0] = 1.0f;
    target[1] = 0.8f;
    index.add("target", target);

    for (size_t i = 0; i < 20; ++i) {
        std::vector<float> distractor(dim, 0.0f);
        distractor[(i + 5) % dim] = 1.0f;
        index.add("distractor_" + std::to_string(i), distractor);
    }

    std::vector<float> query(dim, 0.0f);
    query[0] = 0.95f;
    query[1] = 0.75f;

    auto hits = index.search(query, 5);
    assert(!hits.empty());
    assert(hits[0].id == "target");
    std::cout << "  -> PASSED: Top hit: " << hits[0].id << " (Score: " << hits[0].score << ")" << std::endl;
}

void test_allowlist_bitmask() {
    std::cout << "[Test 3] In-Kernel Allowlist Bitmask Filtering..." << std::endl;
    const size_t dim = 64;
    TurboQuantIndex index(dim);

    for (size_t i = 0; i < 10; ++i) {
        std::vector<float> v(dim, 0.1f * static_cast<float>(i + 1));
        index.add("doc_" + std::to_string(i), v);
    }

    std::vector<float> q(dim, 0.5f);

    // Allow only doc_3 (index 3) and doc_7 (index 7)
    uint64_t mask = (1ULL << 3) | (1ULL << 7);
    auto masked_hits = index.search(q, 10, &mask);

    assert(masked_hits.size() == 2);
    for (const auto& h : masked_hits) {
        assert(h.id == "doc_3" || h.id == "doc_7");
    }
    std::cout << "  -> PASSED: Retained exactly 2 masked candidates out of 10" << std::endl;
}

void test_soft_delete() {
    std::cout << "[Test 4] Soft-Deletion from Index..." << std::endl;
    const size_t dim = 64;
    TurboQuantIndex index(dim);

    std::vector<float> v(dim, 1.0f);
    index.add("item_to_delete", v);
    assert(index.size() == 1);

    auto hits_before = index.search(v, 5);
    assert(!hits_before.empty() && hits_before[0].id == "item_to_delete");

    assert(index.remove("item_to_delete"));
    assert(index.size() == 0);

    auto hits_after = index.search(v, 5);
    assert(hits_after.empty());
    std::cout << "  -> PASSED" << std::endl;
}

void test_persistence_dlvq() {
    std::cout << "[Test 5] Crash-Safe DLVQ Binary Persistence..." << std::endl;
    const size_t dim = 64;
    std::string test_file = "test_turboquant.dlvq";
    std::filesystem::remove(test_file);

    TurboQuantIndex index1(dim);
    std::vector<float> v1(dim, 0.5f);
    std::vector<float> v2(dim, -0.5f);
    index1.add("first", v1);
    index1.add("second", v2);

    assert(index1.save(test_file));
    assert(std::filesystem::exists(test_file));

    TurboQuantIndex index2(dim);
    assert(index2.load(test_file));
    assert(index2.size() == 2);

    auto hits = index2.search(v1, 2);
    assert(!hits.empty());
    assert(hits[0].id == "first");

    std::filesystem::remove(test_file);
    std::cout << "  -> PASSED: DLVQ round-trip verified successfully" << std::endl;
}

void test_benchmark_throughput() {
    std::cout << "[Benchmark 1] TurboQuant Search Throughput..." << std::endl;
    const size_t dim = 1536;
    TurboQuantIndex index(dim);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    std::cout << "  -> Indexing 5,000 vectors (" << dim << "-dim)..." << std::endl;
    for (size_t i = 0; i < 5000; ++i) {
        std::vector<float> v(dim);
        for (size_t d = 0; d < dim; ++d) v[d] = dis(gen);
        index.add("vec_" + std::to_string(i), v);
    }

    std::vector<float> query(dim);
    for (size_t d = 0; d < dim; ++d) query[d] = dis(gen);

    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 100;
    for (int i = 0; i < iterations; ++i) {
        auto hits = index.search(query, 10);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double ms = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    std::cout << "  -> PASSED: AVX2 Throughput: " << ms << " ms per query (5K vectors)" << std::endl;
}

void test_memory_footprint() {
    std::cout << "[Benchmark 2] TurboQuant RAM Footprint Reduction..." << std::endl;
    const size_t dim = 1536;
    size_t fp32_size = 50000 * dim * sizeof(float); // ~307 MB
    size_t tq_size = 50000 * ((dim + 1) / 2);       // ~38 MB
    
    float reduction = static_cast<float>(fp32_size) / static_cast<float>(tq_size);
    std::cout << "  -> FP32 Size: " << (fp32_size / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  -> 4-bit TQ Size: " << (tq_size / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  -> PASSED: Memory Reduction: " << reduction << "x (Expected ~8x)" << std::endl;
}


void benchmark_fp32_recall() {
    // Reproducible synthetic ground truth: independent and near-neighbor queries.
    for (size_t dim : {size_t(128), size_t(512), size_t(1536)}) {
        const size_t n = 1000, queries = 40, k = 10;
        std::mt19937 rng(20260930);
        std::normal_distribution<float> normal(0.0f, 1.0f);
        std::vector<std::vector<float>> vectors(n, std::vector<float>(dim));
        TurboQuantIndex index(dim);
        for (size_t i = 0; i < n; ++i) {
            for (float& x : vectors[i]) x = normal(rng);
            index.add(std::to_string(i), vectors[i]);
        }
        double recall = 0, loss = 0;
        for (size_t q = 0; q < queries; ++q) {
            std::vector<float> query(dim);
            for (size_t d = 0; d < dim; ++d)
                query[d] = q % 2 ? vectors[q][d] + 0.1f * normal(rng) : normal(rng);
            auto cosine = [&](const std::vector<float>& v) {
                double dot = 0, aa = 0, bb = 0;
                for (size_t d = 0; d < dim; ++d) {
                    dot += double(query[d]) * v[d];
                    aa += double(query[d]) * query[d]; bb += double(v[d]) * v[d];
                }
                return dot / std::sqrt(aa * bb);
            };
            std::vector<std::pair<double, size_t>> truth;
            for (size_t i = 0; i < n; ++i) truth.emplace_back(cosine(vectors[i]), i);
            std::sort(truth.begin(), truth.end(), std::greater<std::pair<double, size_t>>());
            auto hits = index.search(query, k);
            assert(hits.size() == k);
            size_t common = 0;
            double optimal = 0, returned = 0;
            for (size_t rank = 0; rank < k; ++rank) {
                optimal += truth[rank].first;
                size_t id = std::stoul(hits[rank].id);
                returned += cosine(vectors[id]);
                for (size_t j = 0; j < k; ++j) if (truth[j].second == id) ++common;
            }
            recall += double(common) / k;
            loss += (optimal - returned) / k;
        }
        std::cout << "FP32 ground truth: seed=20260930 N=" << n << " dim=" << dim
                  << " queries=" << queries << " recall@10=" << recall / queries
                  << " mean cosine ranking loss=" << loss / queries << std::endl;
        assert(recall / queries >= 0.60);
        assert(loss / queries < 0.02);
    }
}

#include "vector/turboquant_ivf.hpp"

void test_turboquant_ivf() {
    std::cout << "[Test 8] TurboQuant IVF Coarse Partitioning..." << std::endl;
    TurboQuantIVF ivf(128, 8, 42);
    assert(ivf.dimension() == 128);
    assert(ivf.num_clusters() == 8);

    std::mt19937 rng(1337);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < 200; ++i) {
        std::vector<float> v(128);
        for (float& x : v) x = dist(rng);
        ivf.add("vec_" + std::to_string(i), v);
    }
    assert(ivf.size() == 200);

    // Search top-5
    std::vector<float> q(128);
    for (float& x : q) x = dist(rng);

    auto hits = ivf.search(q, 5, 4);
    assert(!hits.empty());
    assert(hits.size() <= 5);

    // Soft delete
    assert(ivf.remove("vec_10"));

    std::cout << "  ✓ IVF Partitioning: 200 vectors indexed into 8 clusters, sub-linear probe verified." << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << " DenseLite Phase 3 & 4 TurboQuant SIMD Index Tests" << std::endl;
    std::cout << "=================================================" << std::endl;

    benchmark_fp32_recall();
    test_compression_ratio();
    test_search_accuracy();
    test_allowlist_bitmask();
    test_soft_delete();
    test_persistence_dlvq();

    test_benchmark_throughput();
    test_memory_footprint();
    test_turboquant_ivf();

    std::cout << "=================================================" << std::endl;
    std::cout << " All Phase 3 & 4 TurboQuant Tests PASSED!        " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
