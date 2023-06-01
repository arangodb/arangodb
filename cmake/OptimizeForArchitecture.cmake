# CMAKE_TARGET_ARCHITECTURES -- for backward compatibility
# https://learn.microsoft.com/en-us/windows/win32/winprog64/wow64-implementation-details
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(amd64.*|AMD64.*|x86_64.*)")
  set(ARCH_AMD64 1)
  set(CMAKE_TARGET_ARCHITECTURES "x86_64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64.*|ARM64.*|aarch64.*|AARCH64.*)")
  # armv8.*|ARMV8.* is 32 bit builded aarch64, we don't need to support it now
  # also even if we want to support it, it's more like armv7 with extensions not aarch64
  set(ARCH_AARCH64 1)
  set(CMAKE_TARGET_ARCHITECTURES "aarch64")
  # elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc64le.*|ppc64le.*|PPC64LE.*)")
  # elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(s390x.*|S390X.*)")
  # elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
else ()
  message(WARNING "Platform ${CMAKE_SYSTEM_PROCESSOR} is probably not supported")
endif ()


# Names and design inspired by
# https://github.com/simdjson/simdjson/blob/master/doc/implementation-selection.md
# https://gitlab.com/x86-psABIs/x86-64-ABI/-/blob/master/x86-64-ABI/low-level-sys-info.tex
set(AMD64_V1 -mfxsr -mmmx -msse -msse2)
# "cx16 sse4.2 popcnt pclmul" are specially important
set(AMD64_V2 -mcx16 -msahf -mpopcnt -msse3 -msse4.1 -msse4.2 -mssse3 -mpclmul -mavx -mxsave)
set(AMD64_V3 -mavx2 -mbmi -mbmi2 -mf16c -mfma -mlzcnt -mmovbe)
set(AMD64_V4 -mavx512f -mavx512bw -mavx512cd -mavx512dq -mavx512vl -mavx512vbmi -mavx512vbmi2)

macro(sandybridge)
  message("Optimize for sandybridge")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND ARCHITECTURE_OPTIMIZATIONS /arch:AVX)
  else ()
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V1})
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V2})
  endif ()
endmacro()

macro(haswell)
  message("Optimize for haswell")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND ARCHITECTURE_OPTIMIZATIONS /arch:AVX2)
  else ()
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V1})
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V2})
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V3})
  endif ()
endmacro()

macro(icelake)
  message("Optimize for icelake")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND ARCHITECTURE_OPTIMIZATIONS /arch:AVX512)
  else ()
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V1})
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V2})
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V3})
    list(APPEND ARCHITECTURE_OPTIMIZATIONS ${AMD64_V4})
  endif ()
endmacro()


# Simplified for servers we need to support only AWS Graviton, Ampere Altra, Apple M1
# https://github.com/aws/aws-graviton-getting-started
# https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html#AArch64-Options
macro(graviton1)
  message("Optimize for graviton1")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND ARCHITECTURE_OPTIMIZATIONS /arch:armv8.0)
  else ()
    # little-endian enabled by default, but only if target is not something like aarch64_be
    # so force to generate little-endian code, because it's simpler and less error-prone
    # "fp simd" are included to armv8-a
    list(APPEND ARCHITECTURE_OPTIMIZATIONS -mlittle-endian -march=armv8-a+crc+crypto)
  endif ()
endmacro()

macro(graviton2)
  message("Optimize for graviton2")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND ARCHITECTURE_OPTIMIZATIONS /arch:armv8.2)
  else ()
    list(APPEND ARCHITECTURE_OPTIMIZATIONS -mlittle-endian -march=armv8.2-a+fp16+rcpc+dotprod+crypto)
  endif ()
endmacro()

macro(graviton3)
  message("Optimize for graviton3")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND ARCHITECTURE_OPTIMIZATIONS /arch:armv8.4)
  else ()
    list(APPEND ARCHITECTURE_OPTIMIZATIONS -mlittle-endian -march=armv8.4-a+sve+rng+bf16+int8+crypto)
  endif ()
endmacro()


if (NOT TARGET_ARCHITECTURE OR TARGET_ARCHITECTURE STREQUAL "auto" OR TARGET_ARCHITECTURE STREQUAL "sandybridge" OR TARGET_ARCHITECTURE STREQUAL "sandy-bridge")
  # It's our default mode. empty and auto and sandy-bridge for backward compatibility
  # TODO(MBkkt) iresearch now probably doesn't work without any host specific options, should be fixed?
  if (ARCH_AMD64)
    # We use sandybridge instead of westmere because it's more easy for MSVC, in general difference only in avx and xsave
    sandybridge()
  elseif (ARCH_AARCH64)
    graviton1()
  endif ()
elseif (TARGET_ARCHITECTURE STREQUAL "haswell")
  if (ARCH_AMD64)
    haswell()
  elseif (ARCH_AARCH64)
    graviton2()
  endif ()
elseif (TARGET_ARCHITECTURE STREQUAL "icelake")
  if (ARCH_AMD64)
    icelake()
  elseif (ARCH_AARCH64)
    graviton3()
  endif ()
elseif (TARGET_ARCHITECTURE STREQUAL "native")
  # Mostly for fun :)
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message("Optimize for native")
    list(APPEND ARCHITECTURE_OPTIMIZATIONS -march=native)
  else ()
    message(WARNING "MSVC doesn't support something like -march=native")
  endif ()
else ()
  message(WARNING "${TARGET_ARCHITECTURE} is not supported")
endif ()

string(REPLACE ";" " " ARCHITECTURE_OPTIMIZATIONS "${ARCHITECTURE_OPTIMIZATIONS}")
