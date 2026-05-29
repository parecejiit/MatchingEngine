cmake_minimum_required(VERSION 3.15)
project(MatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "vcpkg toolchain")
endif()

find_package(boost_json CONFIG REQUIRED)
find_package(boost_signals2 CONFIG REQUIRED)
find_package(boost_asio CONFIG REQUIRED)
find_package(tabulate CONFIG REQUIRED)

add_executable(matching_engine
  src/main.cpp
  src/matching_engine.cpp
  src/order_book.cpp
  src/io_adapter.cpp
  src/cli_display.cpp
)

target_include_directories(matching_engine PRIVATE include)
target_compile_features(matching_engine PRIVATE cxx_std_20)
target_link_libraries(matching_engine PRIVATE
  Boost::json
  Boost::signals2
  Boost::asio
  tabulate::tabulate
  $<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
)

if(MSVC)
  target_compile_options(matching_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(matching_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MeDeterministic.cmake)
me_apply_deterministic_build_options(matching_engine)

add_executable(me_benchmark
  src/benchmark.cpp
  src/matching_engine.cpp
  src/order_book.cpp
)
target_include_directories(me_benchmark PRIVATE include)
target_compile_features(me_benchmark PRIVATE cxx_std_20)
target_link_libraries(me_benchmark PRIVATE Boost::json Boost::signals2 Boost::asio)
me_apply_deterministic_build_options(me_benchmark)

enable_testing()
add_test(NAME basic_replay
  COMMAND ${CMAKE_COMMAND} -E env INPUT_FILE=${CMAKE_SOURCE_DIR}/tests/basic.txt
          $<TARGET_FILE:matching_engine>)
add_test(NAME verify_all
  COMMAND ${CMAKE_SOURCE_DIR}/tests/verify_tests.sh)
add_test(NAME determinism
  COMMAND ${CMAKE_SOURCE_DIR}/tests/test_determinism.sh)
set_tests_properties(determinism PROPERTIES
  ENVIRONMENT "ME_BIN=$<TARGET_FILE:matching_engine>")
add_test(NAME market_replay
  COMMAND ${CMAKE_COMMAND} -E env INPUT_FILE=${CMAKE_SOURCE_DIR}/tests/market.txt
          $<TARGET_FILE:matching_engine>)
