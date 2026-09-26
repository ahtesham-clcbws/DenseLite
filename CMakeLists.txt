cmake_minimum_required(VERSION 3.15)
project(DenseLite LANGUAGES CXX)

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
target_link_libraries(DenseLite PRIVATE Threads::Threads sqlite3 zvec_shared OpenMP::OpenMP_CXX OpenSSL::SSL OpenSSL::Crypto)

# Add include directory for headers
target_include_directories(DenseLite PRIVATE src)

# ============================================================================
# Phase 1 Test Targets
# ============================================================================
enable_testing()

add_executable(test_math tests/math_correctness.cpp)
target_include_directories(test_math PRIVATE src)

add_executable(test_config tests/model_config_tests.cpp src/gguf_parser.cpp)
target_include_directories(test_config PRIVATE src)

add_executable(test_golden tests/golden_inference_tests.cpp src/infer.cpp src/gguf_parser.cpp)
target_link_libraries(test_golden PRIVATE OpenMP::OpenMP_CXX)
target_include_directories(test_golden PRIVATE src)

add_test(NAME MathCorrectness COMMAND test_math)
add_test(NAME ModelConfigValidation COMMAND test_config)
add_test(NAME GoldenInference COMMAND test_golden)


