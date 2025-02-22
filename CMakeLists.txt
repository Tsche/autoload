cmake_minimum_required(VERSION 3.15)
project(erl)

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_library(autoload INTERFACE)
target_include_directories(autoload INTERFACE include)

set(CMAKE_CXX_STANDARD 23)
# set(CMAKE_CXX_STANDARD 26)
# target_compile_options(autoload INTERFACE "-stdlib=libc++" "-freflection")
# target_link_options(autoload INTERFACE "-stdlib=libc++" "-lc++abi")

option(BUILD_TESTING "Enable tests" OFF)
option(BUILD_EXAMPLES "Enable examples" ON)
option(ENABLE_COVERAGE "Enable coverage instrumentation" OFF)

if (BUILD_TESTING)
  message(STATUS "Building unit tests")

  enable_testing()
  add_executable(autoload_tests "")
  add_subdirectory(tests)

  find_package(GTest REQUIRED)
  target_link_libraries(autoload_tests PRIVATE autoload)
  target_link_libraries(autoload_tests PRIVATE GTest::gtest GTest::gmock)

  include(GoogleTest)
  gtest_discover_tests(autoload_tests)


  if(ENABLE_COVERAGE)
    message(STATUS "Instrumenting for coverage")  

    target_compile_options(autoload_tests PRIVATE -g -O0 --coverage)
    target_link_libraries(autoload_tests PRIVATE --coverage)
  endif()
endif()

if (BUILD_EXAMPLES)
  add_subdirectory(example)
endif()