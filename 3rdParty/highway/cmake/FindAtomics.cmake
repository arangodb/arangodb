# Original issue:
# * https://gitlab.kitware.com/cmake/cmake/-/issues/23021#note_1098733
#
# For reference:
# * https://gcc.gnu.org/wiki/Atomic/GCCMM
#
# riscv64 specific:
# * https://lists.debian.org/debian-riscv/2022/01/msg00009.html
#
# ATOMICS_FOUND        - system has c++ atomics
# ATOMICS_LIBRARIES    - libraries needed to use c++ atomics

include(CheckCXXSourceCompiles)

# RISC-V only has 32-bit and 64-bit atomic instructions. GCC is supposed
# to convert smaller atomics to those larger ones via masking and
# shifting like LLVM, but it’s a known bug that it does not. This means
# anything that wants to use atomics on 1-byte or 2-byte types needs
# -latomic, but not 4-byte or 8-byte (though it does no harm).
set(atomic_code
    "
     #include <atomic>
     #include <cstdint>
     std::atomic<uint8_t> n8 (0); // riscv64
     std::atomic<uint64_t> n64 (0); // armel, mipsel, powerpc
     int main() {
       ++n8;
       ++n64;
       return 0;
     }")

# https://gitlab.kitware.com/cmake/cmake/-/issues/24063
set(CMAKE_CXX_STANDARD 11)
check_cxx_source_compiles("${atomic_code}" ATOMICS_LOCK_FREE_INSTRUCTIONS)

if(ATOMICS_LOCK_FREE_INSTRUCTIONS)
  set(ATOMICS_FOUND TRUE)
  set(ATOMICS_LIBRARIES)
else()
  set(CMAKE_REQUIRED_LIBRARIES "-latomic")
  check_cxx_source_compiles("${atomic_code}" ATOMICS_IN_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES)
  if(ATOMICS_IN_LIBRARY)
    set(ATOMICS_LIBRARY atomic)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Atomics DEFAULT_MSG ATOMICS_LIBRARY)
    set(ATOMICS_LIBRARIES ${ATOMICS_LIBRARY})
    unset(ATOMICS_LIBRARY)
  else()
    if(Atomics_FIND_REQUIRED)
      message(FATAL_ERROR "Neither lock free instructions nor -latomic found.")
    endif()
  endif()
endif()
unset(atomic_code)
unset(CMAKE_CXX_STANDARD)
