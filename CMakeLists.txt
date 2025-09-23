# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(bykey VERSION 1.0.0 LANGUAGES CXX)
add_library(bykey INTERFACE)
target_include_directories(bykey INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(bykey INTERFACE cxx_std_20)
add_library(bykey::bykey ALIAS bykey)

option(BUILD_EXAMPLES "Build examples" ON)
if (BUILD_EXAMPLES)
  function(add_example name)
    add_executable(${name} examples/${name}.cpp)
    target_link_libraries(${name} PRIVATE bykey::bykey)
    target_compile_features(${name} PRIVATE cxx_std_20)
  endfunction()

  add_example(lc_0049_group_anagrams)
  add_example(lc_0347_top_k_frequent)
  add_example(lc_0697_degree_of_array)
  add_example(lc_0350_intersection_ii)
  add_example(lc_0242_valid_anagram)
  add_example(lc_1331_rank_transform)
endif()


include(CTest)              # provides BUILD_TESTING option (ON by default in CI below)
if (BUILD_TESTING)
  enable_testing()
  include(FetchContent)

  # Option to use system GTest if you want (Arch has a package)
  option(USE_SYSTEM_GTEST "Use system-installed GoogleTest" OFF)

  if (USE_SYSTEM_GTEST)
    find_package(GTest CONFIG REQUIRED) # or: find_package(GTest REQUIRED)
  else()
    # Pin a known-good tag
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    # (MSVC CRT tweak is harmless elsewhere)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(bykey_tests tests/test_by_key.cpp)
  target_link_libraries(bykey_tests PRIVATE bykey GTest::gtest_main)
  target_compile_features(bykey_tests PRIVATE cxx_std_20)

  include(GoogleTest)
  gtest_discover_tests(bykey_tests)
endif()
