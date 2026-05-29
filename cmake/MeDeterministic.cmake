# Deterministic, reproducible matching: forbid FP/unsafe math opts; optional -O0 verify build.
#
# Usage (from CMakeLists.txt):
#   include(MeDeterministic)
#   me_apply_deterministic_build_options(matching_engine)

option(ME_DISABLE_COMPILER_OPTIMIZATION
  "Build engine with -O0 (/Od) for golden replay verification (slower; same match semantics)"
  OFF)

function(me_apply_deterministic_build_options target)
  if(MSVC)
    # /fp:strict — no fast-math; IEEE-friendly conversions at JSON boundary
    target_compile_options(${target} PRIVATE /fp:strict)
    if(ME_DISABLE_COMPILER_OPTIMIZATION OR CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${target} PRIVATE /Od)
    else()
      target_compile_options(${target} PRIVATE /O2)
    endif()
  else()
    # Never use -Ofast or -ffast-math: price_to_ticks uses double + llround on the cold path.
    target_compile_options(${target} PRIVATE
      -fno-fast-math
      -ffp-contract=off
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target} PRIVATE
        -fno-unsafe-math-optimizations
        -fno-associative-math
        -fno-reciprocal-math
      )
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${target} PRIVATE -fno-approx-func)
    endif()

    if(ME_DISABLE_COMPILER_OPTIMIZATION OR CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${target} PRIVATE -O0)
    else()
      target_compile_options(${target} PRIVATE -O3)
    endif()
  endif()

  target_compile_definitions(${target} PRIVATE
    $<$<BOOL:${ME_DISABLE_COMPILER_OPTIMIZATION}>:ME_BUILD_NO_COMPILER_OPT=1>
  )
endfunction()
