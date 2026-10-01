cmake_minimum_required(VERSION 3.15)
project(DenseLite VERSION 4.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
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
    src/vulkan_compute.cpp
    src/model_registry.cpp
    src/model_pool.cpp
    src/model_lease.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/resource_governor.cpp
    src/model_manager.cpp
    src/RequestAnalyzer.cpp
    src/decision_engine.cpp
    src/Formatter.cpp
    src/Router.cpp
    src/routing/modernbert_router.cpp
    src/ProviderErrorAnalyzer.cpp
    src/DenseLiteEngine.cpp
    src/sqlite_router.cpp
    src/sqlite_router_models.cpp
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
    src/nli_evaluator.cpp
    src/memory_engine.cpp
    src/language_registry.cpp
    src/ast_chunker.cpp
    src/symbol_index.cpp
    src/code_change_tracker.cpp
    src/code_indexer.cpp
    src/exact_search.cpp
    src/lexical_search.cpp
    src/vector_search.cpp
    src/vector/turboquant_index.cpp
    src/vector/turboquant_ivf.cpp
    src/structural_search.cpp
    src/result_fusion.cpp
    src/search_engine.cpp
    src/multimodal_engine.cpp
    src/SessionToolRegistry.cpp
    src/SessionKVCache.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
    src/databases/database_migrator.cpp
    src/model_inspector.cpp
    src/settings/model_registry_db.cpp
    src/settings/model_discovery.cpp
    src/model_cli.cpp
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

set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/onnxruntime")
target_compile_definitions(DenseLite PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_directories(DenseLite PRIVATE ${ONNXRUNTIME_DIR}/lib)
target_link_libraries(DenseLite PRIVATE Threads::Threads sqlite3 zvec_shared OpenMP::OpenMP_CXX OpenSSL::SSL OpenSSL::Crypto vulkan treesitter_static onnxruntime)
set_target_properties(DenseLite PROPERTIES BUILD_RPATH "${ONNXRUNTIME_DIR}/lib" INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")

# Add include directory for headers
target_include_directories(DenseLite PRIVATE 
    src 
    src/settings 
    src/databases 
    src/routing
    dependencies
    dependencies/vulkan/include 
    dependencies/tree-sitter/lib/include
    ${ONNXRUNTIME_DIR}/include
)

# ============================================================================
# Native System Tray & Control Plane (DenseLiteTray)
# ============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(AYATANA REQUIRED ayatana-appindicator3-0.1 gtk+-3.0)

add_executable(DenseLiteTray
    src/tray/main_tray.cpp
    src/tray/tray_app.cpp
    src/tray/tray_process.cpp
    src/tray/tray_server.cpp
    src/tray/tray_routes_settings.cpp
    src/tray/tray_routes_system.cpp
    src/tray/tray_routes_models.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
    src/settings/model_registry_db.cpp
    src/settings/model_discovery.cpp
    src/databases/database_migrator.cpp
    src/model_inspector.cpp
)
target_include_directories(DenseLiteTray PRIVATE 
    src 
    src/settings 
    src/databases 
    src/tray
    dependencies
    ${AYATANA_INCLUDE_DIRS}
)
target_compile_definitions(DenseLiteTray PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(DenseLiteTray PRIVATE 
    Threads::Threads 
    sqlite3 
    OpenSSL::SSL 
    OpenSSL::Crypto
    ${AYATANA_LIBRARIES}
)
target_compile_options(DenseLiteTray PRIVATE ${AYATANA_CFLAGS_OTHER})

# ============================================================================
# Test Targets (Phase 1 & Phase 2)
# ============================================================================
enable_testing()

add_executable(test_math tests/math_correctness.cpp src/vulkan_device.cpp src/vulkan_compute.cpp)
target_link_libraries(test_math PRIVATE vulkan)
target_include_directories(test_math PRIVATE src dependencies/vulkan/include)

add_executable(test_config tests/model_config_tests.cpp src/gguf_parser.cpp)
target_include_directories(test_config PRIVATE src)

add_executable(test_golden tests/golden_inference_tests.cpp src/infer.cpp src/gguf_parser.cpp src/vulkan_device.cpp src/vulkan_compute.cpp)
target_link_libraries(test_golden PRIVATE OpenMP::OpenMP_CXX vulkan)
target_include_directories(test_golden PRIVATE src dependencies/vulkan/include)

add_executable(test_lifecycle tests/model_lifecycle_tests.cpp 
    src/vulkan_device.cpp
    src/vulkan_compute.cpp
    src/model_registry.cpp
    src/model_pool.cpp
    src/model_lease.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/resource_governor.cpp
    src/model_manager.cpp
    src/gguf_parser.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
    src/settings/model_registry_db.cpp
    src/settings/model_discovery.cpp
    src/model_inspector.cpp
    src/tokenizer.cpp
    src/routing/modernbert_router.cpp
)
target_link_directories(test_lifecycle PRIVATE ${ONNXRUNTIME_DIR}/lib)
target_link_libraries(test_lifecycle PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan sqlite3 onnxruntime)
set_target_properties(test_lifecycle PROPERTIES BUILD_RPATH "${ONNXRUNTIME_DIR}/lib" INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")
target_include_directories(test_lifecycle PRIVATE src src/settings src/databases src/routing dependencies dependencies/vulkan/include ${ONNXRUNTIME_DIR}/include)

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
    src/vulkan_compute.cpp
    src/model_registry.cpp
    src/model_pool.cpp
    src/model_lease.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/model_manager.cpp
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
    src/nli_evaluator.cpp
    src/memory_engine.cpp
    src/language_registry.cpp
    src/ast_chunker.cpp
    src/symbol_index.cpp
    src/code_change_tracker.cpp
    src/code_indexer.cpp
    src/exact_search.cpp
    src/lexical_search.cpp
    src/vector_search.cpp
    src/vector/turboquant_index.cpp
    src/vector/turboquant_ivf.cpp
    src/structural_search.cpp
    src/result_fusion.cpp
    src/search_engine.cpp
    src/ResponseAnalyzer.cpp
    src/RecoveryPolicy.cpp
    src/CompletionPolicy.cpp
    src/Curator.cpp
    src/multimodal_engine.cpp
    src/SessionToolRegistry.cpp
    src/SessionKVCache.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
    src/settings/model_registry_db.cpp
    src/settings/model_discovery.cpp
    src/model_inspector.cpp
    src/infer.cpp
    src/routing/modernbert_router.cpp
)
target_link_directories(benchmark_engine PRIVATE ${ONNXRUNTIME_DIR}/lib)
target_link_libraries(benchmark_engine PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan sqlite3 treesitter_static onnxruntime)
set_target_properties(benchmark_engine PROPERTIES BUILD_RPATH "${ONNXRUNTIME_DIR}/lib" INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")
target_include_directories(benchmark_engine PRIVATE src src/settings src/databases src/routing dependencies dependencies/vulkan/include dependencies/tree-sitter/lib/include ${ONNXRUNTIME_DIR}/include)

add_executable(test_memory tests/memory_engine_tests.cpp
    src/working_memory.cpp
    src/session_memory.cpp
    src/persistent_memory.cpp
    src/memory_store.cpp
    src/memory_recall.cpp
    src/memory_consolidator.cpp
    src/nli_evaluator.cpp
    src/memory_engine.cpp
    src/vector_search.cpp
    src/vector/turboquant_index.cpp
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
    src/vector/turboquant_index.cpp
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
    src/nli_evaluator.cpp
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
    src/vulkan_compute.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
)
target_link_libraries(test_governor PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan sqlite3)
target_include_directories(test_governor PRIVATE src src/settings src/databases dependencies/vulkan/include)
add_test(NAME ResourceGovernor COMMAND test_governor)

add_executable(test_multimodal tests/multimodal_tests.cpp
    src/multimodal_engine.cpp
    src/resource_governor.cpp
    src/vulkan_device.cpp
    src/vulkan_compute.cpp
    src/RequestAnalyzer.cpp
    src/model_manager.cpp
    src/model_registry.cpp
    src/model_pool.cpp
    src/model_lease.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/gguf_parser.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
    src/settings/model_registry_db.cpp
    src/settings/model_discovery.cpp
    src/model_inspector.cpp
    src/tokenizer.cpp
    src/routing/modernbert_router.cpp
)
target_link_directories(test_multimodal PRIVATE ${ONNXRUNTIME_DIR}/lib)
target_link_libraries(test_multimodal PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan sqlite3 onnxruntime)
set_target_properties(test_multimodal PROPERTIES BUILD_RPATH "${ONNXRUNTIME_DIR}/lib" INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")
target_include_directories(test_multimodal PRIVATE src src/settings src/databases src/routing dependencies dependencies/vulkan/include ${ONNXRUNTIME_DIR}/include)
add_test(NAME MultimodalEngine COMMAND test_multimodal)

add_executable(test_session_kv tests/session_kv_tests.cpp
    src/SessionToolRegistry.cpp
    src/SessionKVCache.cpp
    src/settings/path_service.cpp
    src/infer.cpp
    src/gguf_parser.cpp
    src/vulkan_device.cpp
    src/vulkan_compute.cpp
)
target_link_libraries(test_session_kv PRIVATE Threads::Threads OpenMP::OpenMP_CXX vulkan)
target_include_directories(test_session_kv PRIVATE src src/settings dependencies/vulkan/include)
add_test(NAME SessionKVCache COMMAND test_session_kv)

add_executable(test_settings tests/settings_tests.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
    src/databases/database_migrator.cpp
    src/settings/model_registry_db.cpp
    src/model_inspector.cpp
)
target_link_libraries(test_settings PRIVATE Threads::Threads sqlite3)
target_include_directories(test_settings PRIVATE src src/settings src/databases)
add_test(NAME SettingsEngine COMMAND test_settings)

add_executable(test_model_registry tests/model_registry_tests.cpp
    src/settings/settings_db.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_dtos.cpp
    src/settings/path_service.cpp
    src/databases/database_migrator.cpp
    src/settings/model_registry_db.cpp
    src/settings/model_discovery.cpp
    src/model_inspector.cpp
)
target_link_libraries(test_model_registry PRIVATE Threads::Threads sqlite3)
target_include_directories(test_model_registry PRIVATE src src/settings src/databases)
add_test(NAME ModelRegistry COMMAND test_model_registry)

add_executable(test_decision tests/decision_engine_tests.cpp
    src/decision_engine.cpp
    src/RequestAnalyzer.cpp
    src/routing/modernbert_router.cpp
    src/settings/path_service.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_db.cpp
    src/settings/settings_dtos.cpp
    src/databases/database_migrator.cpp
    src/Formatter.cpp
    src/tokenizer.cpp
)
target_link_directories(test_decision PRIVATE ${ONNXRUNTIME_DIR}/lib)
target_link_libraries(test_decision PRIVATE Threads::Threads sqlite3 onnxruntime)
set_target_properties(test_decision PROPERTIES BUILD_RPATH "${ONNXRUNTIME_DIR}/lib" INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")
target_include_directories(test_decision PRIVATE src src/settings src/databases src/routing dependencies ${ONNXRUNTIME_DIR}/include)
add_test(NAME DecisionEngine COMMAND test_decision)

add_executable(test_turboquant tests/turboquant_tests.cpp
    src/vector/turboquant_index.cpp
    src/vector/turboquant_ivf.cpp
)
target_link_libraries(test_turboquant PRIVATE Threads::Threads)
target_include_directories(test_turboquant PRIVATE src)
add_test(NAME TurboQuant COMMAND test_turboquant)

add_executable(test_server_integration tests/server_integration_tests.cpp)
target_link_libraries(test_server_integration PRIVATE Threads::Threads)
target_include_directories(test_server_integration PRIVATE src)
add_test(NAME ServerIntegration COMMAND test_server_integration)

# Quality & Reality Validation Benchmarks (Points 2, 6, 8, 9, 10, 16)
add_executable(test_turboquant_quality tests/test_turboquant_quality.cpp
    src/vector/turboquant_index.cpp
    src/vector/turboquant_ivf.cpp
)
target_link_libraries(test_turboquant_quality PRIVATE Threads::Threads)
target_include_directories(test_turboquant_quality PRIVATE src)
add_test(NAME TurboQuantQuality COMMAND test_turboquant_quality)

add_executable(test_completion_quality tests/test_completion_quality.cpp
    src/CompletionPolicy.cpp
    src/ResponseAnalyzer.cpp
)
target_link_libraries(test_completion_quality PRIVATE Threads::Threads)
target_include_directories(test_completion_quality PRIVATE src dependencies)
add_test(NAME CompletionQuality COMMAND test_completion_quality)

add_executable(test_decision_quality tests/test_decision_engine_quality.cpp
    src/decision_engine.cpp
    src/RequestAnalyzer.cpp
    src/routing/modernbert_router.cpp
    src/settings/path_service.cpp
    src/settings/settings_manager.cpp
    src/settings/settings_db.cpp
    src/settings/settings_dtos.cpp
    src/databases/database_migrator.cpp
    src/Formatter.cpp
    src/tokenizer.cpp
)
target_link_directories(test_decision_quality PRIVATE ${ONNXRUNTIME_DIR}/lib)
target_link_libraries(test_decision_quality PRIVATE Threads::Threads sqlite3 onnxruntime)
set_target_properties(test_decision_quality PROPERTIES BUILD_RPATH "${ONNXRUNTIME_DIR}/lib" INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib")
target_include_directories(test_decision_quality PRIVATE src src/settings src/databases src/routing dependencies ${ONNXRUNTIME_DIR}/include)
add_test(NAME DecisionEngineQuality COMMAND test_decision_quality)

add_executable(test_memory_quality tests/test_memory_quality.cpp
    src/working_memory.cpp
    src/session_memory.cpp
    src/persistent_memory.cpp
    src/memory_store.cpp
    src/memory_recall.cpp
    src/memory_consolidator.cpp
    src/nli_evaluator.cpp
    src/memory_engine.cpp
    src/vector_search.cpp
    src/vector/turboquant_index.cpp
)
target_link_libraries(test_memory_quality PRIVATE Threads::Threads sqlite3)
target_include_directories(test_memory_quality PRIVATE src)
add_test(NAME MemoryConsolidationQuality COMMAND test_memory_quality)

add_executable(eval_64k_production benchmarks/eval_64k_production.cpp
    src/kv_cache.cpp
    src/vulkan_device.cpp
    src/vulkan_compute.cpp
)
target_link_libraries(eval_64k_production PRIVATE Threads::Threads vulkan)
target_include_directories(eval_64k_production PRIVATE src dependencies/vulkan/include)
add_test(NAME Eval64KProduction COMMAND eval_64k_production)

add_executable(test_resident_pool_fit benchmarks/test_resident_pool_fit.cpp
    src/kv_cache.cpp
    src/vulkan_device.cpp
    src/vulkan_compute.cpp
)
target_link_libraries(test_resident_pool_fit PRIVATE Threads::Threads vulkan)
target_include_directories(test_resident_pool_fit PRIVATE src dependencies dependencies/vulkan/include)
add_test(NAME ResidentPoolCapacityFit COMMAND test_resident_pool_fit)

add_test(NAME ModelChecksumCoverage COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_model_checksum_coverage.py)

# ==============================================================================
# Installation Targets (Portability & Packaging - IDs 170, 200, 214)
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS DenseLite DenseLiteTray
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY src/databases/migrations/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/denselite/migrations
    FILES_MATCHING PATTERN "*.sql"
)

install(DIRECTORY web/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/denselite/web
    OPTIONAL
)

install(FILES env.example
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/denselite
    RENAME denselite.env.example
)

