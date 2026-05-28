cmake_minimum_required(VERSION 3.15)
project(lockfree VERSION 1.0.0 LANGUAGES CXX)

# C++17 必须
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release 模式优化
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /MT")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
endif()


# 头文件库
add_library(lockfree INTERFACE)
target_include_directories(lockfree INTERFACE include)

# demo 程序
add_executable(spsc_demo examples/02_spsc_demo.cpp)
target_link_libraries(spsc_demo lockfree)

add_executable(mpmc_demo examples/04_mpmc_demo.cpp)
target_link_libraries(mpmc_demo lockfree)

#tagged_ptr单元测试 (Google Test)
add_executable(test_tagged_ptr
  tests/test_tagged_ptr.cpp
)
target_include_directories(test_tagged_ptr PRIVATE
  ${googletest_SOURCE_DIR}/googletest/include
  ${googletest_SOURCE_DIR}/googlemock/include
)
target_link_libraries(test_tagged_ptr lockfree gtest_main gmock_main)
add_test(NAME tagged_ptr_test COMMAND test_tagged_ptr)

#MSQ demo
add_executable(msq_demo examples/05_msq_demo.cpp)
target_link_libraries(msq_demo lockfree)

# ── Benchmark ────────────────────────────
add_executable(bench_spsc benchmark/bench_spsc.cpp)
target_include_directories(bench_spsc PRIVATE benchmark)
target_link_libraries(bench_spsc lockfree)

add_executable(bench_mpmc_bounded benchmark/bench_mpmc_bounded.cpp)
target_include_directories(bench_mpmc_bounded PRIVATE benchmark)
target_link_libraries(bench_mpmc_bounded lockfree)

add_executable(bench_mpmc_unbounded benchmark/bench_mpmc_unbounded.cpp)
target_include_directories(bench_mpmc_unbounded PRIVATE benchmark)
target_link_libraries(bench_mpmc_unbounded lockfree)

# ============================================================
# Google Test (FetchContent)
# ============================================================
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# 避免 gtest 编译自己的 main
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(test_spsc
  tests/test_spsc.cpp
)

target_include_directories(test_spsc PRIVATE
  ${googletest_SOURCE_DIR}/googletest/include
  ${googletest_SOURCE_DIR}/googlemock/include
)

target_link_libraries(test_spsc lockfree gtest_main gmock_main)
add_test(NAME spsc_test COMMAND test_spsc)

add_executable(test_mpmc_bounded
  tests/test_mpmc_bounded.cpp
)
target_include_directories(test_mpmc_bounded PRIVATE
  ${googletest_SOURCE_DIR}/googletest/include
  ${googletest_SOURCE_DIR}/googlemock/include
)
target_link_libraries(test_mpmc_bounded lockfree gtest_main gmock_main)
add_test(NAME mpmc_bounded_test COMMAND test_mpmc_bounded)

add_executable(test_mpmc_unbounded
  tests/test_mpmc_unbounded.cpp
)
target_include_directories(test_mpmc_unbounded PRIVATE
  ${googletest_SOURCE_DIR}/googletest/include
  ${googletest_SOURCE_DIR}/googlemock/include
)
target_link_libraries(test_mpmc_unbounded lockfree gtest_main gmock_main)
add_test(NAME mpmc_unbounded_test COMMAND test_mpmc_unbounded)

# ============================================================
# Sanitizer 选项（默认关闭）
#
# 用法:
#   cmake -DENABLE_ASAN=ON  ..
#   cmake -DENABLE_TSAN=ON  ..   (仅 clang/gcc，MSVC 不支持 TSan)
# ============================================================
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN)
  if(MSVC)
    add_compile_options(/fsanitize=address)
    add_link_options(/fsanitize=address)
  else()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
  endif()
  message(STATUS "AddressSanitizer enabled")
endif()

if(ENABLE_TSAN)
  if(NOT MSVC)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
    message(STATUS "ThreadSanitizer enabled")
  else()
    message(FATAL_ERROR "ThreadSanitizer is not supported on MSVC. Use clang/gcc instead.")
  endif()
endif()
