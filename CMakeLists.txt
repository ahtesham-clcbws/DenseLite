cmake_minimum_required(VERSION 3.15)
project(DenseLite LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enforce pure AVX2 and maximum optimizations (-O3, -mfma) for our 100% custom math kernels.
# We explicitly do NOT link to OpenBLAS or MKL here as per our zero-dependency architecture.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mavx2 -mfma -mf16c -Wall -Wextra -Wpedantic")

# Optionally add threading support if we need std::thread later for 2-core parallel execution.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# The core DenseLite engine executable
add_executable(DenseLite 
    src/server.cpp
    src/gguf_parser.cpp
    src/infer.cpp
    src/vulkan_device.cpp
    src/model_registry.cpp
    src/model_pool.cpp
    src/model_lease.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/resource_governor.cpp
    src/model_manager.cpp
    src/RequestAnalyzer.cpp
    src/Formatter.cpp
    src/NeedleRouter.cpp
    src/ProviderErrorAnalyzer.cpp
    src/DenseLiteEngine.cpp
    src/sqlite_router.cpp
    src/ContextManager.cpp
    src/Curator.cpp
    src/ModelEngine.cpp
    src/ResponseAnalyzer.cpp
    src/RecoveryPolicy.cpp
    src/CompletionPolicy.cpp
    src/ProviderSync.cpp
    src/tokenizer.cpp
    src/tokenizer_registry.cpp
    src/context_budgeter.cpp
    src/context_compressor.cpp
    src/context_compiler.cpp
    src/context_engine.cpp
    src/working_memory.cpp
    src/session_memory.cpp
    src/persistent_memory.cpp
    src/memory_store.cpp
    src/memory_recall.cpp
    src/memory_consolidator.cpp
    src/memory_engine.cpp
    src/language_registry.cpp
    src/ast_chunker.cpp
    src/symbol_index.cpp
    src/code_change_tracker.cpp
    src/code_indexer.cpp
    src/exact_search.cpp
    src/lexical_search.cpp
    src/vector_search.cpp
    src/structural_search.cpp
    src/result_fusion.cpp
    src/search_engine.cpp
)

# Tree-sitter Core Runtime & Multi-Language Grammars
add_library(treesitter_static STATIC
    dependencies/tree-sitter/lib/src/lib.c
    dependencies/tree-sitter/grammars/cpp/src/parser.c
    dependencies/tree-sitter/grammars/cpp/src/scanner.c
    dependencies/tree-sitter/grammars/python/src/parser.c
    dependencies/tree-sitter/grammars/python/src/scanner.c
    dependencies/tree-sitter/grammars/php/php/src/parser.c
    dependencies/tree-sitter/grammars/php/php/src/scanner.c
    dependencies/tree-sitter/grammars/javascript/src/parser.c
    dependencies/tree-sitter/grammars/javascript/src/scanner.c
)
target_include_directories(treesitter_static PUBLIC
    dependencies/tree-sitter/lib/include
    dependencies/tree-sitter/grammars/cpp/src
    dependencies/tree-sitter/grammars/python/src
    dependencies/tree-sitter/grammars/php/php/src
    dependencies/tree-sitter/grammars/php/common
    dependencies/tree-sitter/grammars/javascript/src
)

# Disable massive Zvec submodules building to keep DenseLite fast
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_CPP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
set(BUILD_C_BINDINGS OFF CACHE BOOL "" FORCE)

add_subdirectory(dependencies/zvec)

find_package(OpenMP REQUIRED)
find_package(OpenSSL REQUIRED)

target_compile_definitions(DenseLite PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(DenseLite PRIVATE Threads::Threads sqlite3 zvec_shared OpenMP::OpenMP_CXX OpenSSL::SSL OpenSSL::Crypto vulkan treesitter_static)

# Add include directory for headers
target_include_directories(DenseLite PRIVATE src dependencies/vulkan/include dependencies/tree-sitter/lib/include)

# ============================================================================
# Test Targets (Phase 1 & Phase 2)
# ============================================================================
enable_testing()

add_executable(test_math tests/math_correctness.cpp)
target_include_directories(test_math PRIVATE src)

add_executable(test_config tests/model_config_tests.cpp src/gguf_parser.cpp)
target_include_directories(test_config PRIVATE src)

add_executable(test_golden tests/golden_inference_tests.cpp src/infer.cpp src/gguf_parser.cpp)
target_link_libraries(test_golden PRIVATE OpenMP::OpenMP_CXX)
target_include_directories(test_golden PRIVATE src)

add_executable(test_lifecycle tests/model_lifecycle_tests.cpp 
    src/vulkan_device.cpp
    src/model_registry.cpp
    src/model_pool.cpp
    src/model_lease.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/resource_governor.cpp
    src/model_manager.cpp
    src/gguf_parser.cpp
)
target_link_libraries(test_lifecycle PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan)
target_include_directories(test_lifecycle PRIVATE src dependencies/vulkan/include)

add_test(NAME MathCorrectness COMMAND test_math)
add_test(NAME ModelConfigValidation COMMAND test_config)
add_test(NAME GoldenInference COMMAND test_golden)
add_test(NAME ModelLifecycle COMMAND test_lifecycle)

add_executable(test_context tests/context_engine_tests.cpp
    src/tokenizer.cpp
    src/tokenizer_registry.cpp
    src/context_budgeter.cpp
    src/context_compressor.cpp
    src/context_compiler.cpp
    src/context_engine.cpp
    src/RequestAnalyzer.cpp
)
target_link_libraries(test_context PRIVATE Threads::Threads)
target_include_directories(test_context PRIVATE src)
add_test(NAME ContextEngine COMMAND test_context)

add_executable(benchmark_engine benchmarks/benchmark_engine.cpp
    src/vulkan_device.cpp
    src/model_registry.cpp
    src/model_pool.cpp
    src/model_lease.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/resource_governor.cpp
    src/gguf_parser.cpp
    src/tokenizer.cpp
    src/tokenizer_registry.cpp
    src/context_budgeter.cpp
    src/context_compressor.cpp
    src/context_compiler.cpp
    src/context_engine.cpp
    src/RequestAnalyzer.cpp
    src/working_memory.cpp
    src/session_memory.cpp
    src/persistent_memory.cpp
    src/memory_store.cpp
    src/memory_recall.cpp
    src/memory_consolidator.cpp
    src/memory_engine.cpp
    src/language_registry.cpp
    src/ast_chunker.cpp
    src/symbol_index.cpp
    src/code_change_tracker.cpp
    src/code_indexer.cpp
    src/exact_search.cpp
    src/lexical_search.cpp
    src/vector_search.cpp
    src/structural_search.cpp
    src/result_fusion.cpp
    src/search_engine.cpp
    src/ResponseAnalyzer.cpp
    src/RecoveryPolicy.cpp
    src/CompletionPolicy.cpp
    src/Curator.cpp
)
target_link_libraries(benchmark_engine PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan sqlite3 treesitter_static)
target_include_directories(benchmark_engine PRIVATE src dependencies/vulkan/include dependencies/tree-sitter/lib/include)

add_executable(test_memory tests/memory_engine_tests.cpp
    src/working_memory.cpp
    src/session_memory.cpp
    src/persistent_memory.cpp
    src/memory_store.cpp
    src/memory_recall.cpp
    src/memory_consolidator.cpp
    src/memory_engine.cpp
)
target_link_libraries(test_memory PRIVATE Threads::Threads sqlite3)
target_include_directories(test_memory PRIVATE src)
add_test(NAME MemoryEngine COMMAND test_memory)

add_executable(test_code_intel tests/code_indexer_tests.cpp
    src/language_registry.cpp
    src/ast_chunker.cpp
    src/symbol_index.cpp
    src/code_change_tracker.cpp
    src/code_indexer.cpp
)
target_link_libraries(test_code_intel PRIVATE Threads::Threads sqlite3 treesitter_static)
target_include_directories(test_code_intel PRIVATE src dependencies/tree-sitter/lib/include)
add_test(NAME CodeIntelligence COMMAND test_code_intel)

add_executable(test_search tests/search_engine_tests.cpp
    src/exact_search.cpp
    src/lexical_search.cpp
    src/vector_search.cpp
    src/structural_search.cpp
    src/result_fusion.cpp
    src/search_engine.cpp
    src/language_registry.cpp
    src/ast_chunker.cpp
    src/symbol_index.cpp
    src/code_change_tracker.cpp
    src/code_indexer.cpp
    src/working_memory.cpp
    src/session_memory.cpp
    src/persistent_memory.cpp
    src/memory_store.cpp
    src/memory_recall.cpp
    src/memory_consolidator.cpp
    src/memory_engine.cpp
    src/tokenizer.cpp
    src/tokenizer_registry.cpp
    src/context_budgeter.cpp
    src/context_compressor.cpp
    src/context_compiler.cpp
    src/context_engine.cpp
)
target_link_libraries(test_search PRIVATE Threads::Threads sqlite3 treesitter_static)
target_include_directories(test_search PRIVATE src dependencies/tree-sitter/lib/include)
add_test(NAME SearchEngine COMMAND test_search)

add_executable(test_agent_loop tests/agent_loop_tests.cpp
    src/ResponseAnalyzer.cpp
    src/RecoveryPolicy.cpp
    src/CompletionPolicy.cpp
    src/Curator.cpp
)
target_link_libraries(test_agent_loop PRIVATE Threads::Threads)
target_include_directories(test_agent_loop PRIVATE src)
add_test(NAME AgentLoop COMMAND test_agent_loop)

add_executable(test_governor tests/resource_governor_tests.cpp
    src/resource_governor.cpp
    src/vulkan_device.cpp
)
target_link_libraries(test_governor PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan)
target_include_directories(test_governor PRIVATE src dependencies/vulkan/include)
add_test(NAME ResourceGovernor COMMAND test_governor)










