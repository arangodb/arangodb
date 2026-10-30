# -*- mode: CMAKE; -*-
# mimalloc integration for ArangoDB.
#
# Unlike jemalloc (autotools, built via ExternalProject), mimalloc is a native
# CMake project, so we can consume it directly via add_subdirectory. We build
# only the static library and let it override the global allocator for the whole
# process, exactly as jemalloc does today.

# Build just the static override library -- no shared lib, object lib or tests.
set(MI_BUILD_SHARED OFF CACHE BOOL "mimalloc: build shared library" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "mimalloc: build object library" FORCE)
set(MI_BUILD_TESTS  OFF CACHE BOOL "mimalloc: build tests" FORCE)
set(MI_BUILD_STATIC ON  CACHE BOOL "mimalloc: build static library" FORCE)
set(MI_OVERRIDE     ON  CACHE BOOL "mimalloc: override the standard malloc interface" FORCE)

# EXCLUDE_FROM_ALL keeps mimalloc's own install() rules and unused targets out of
# our package; mimalloc-static is still built because arangod links against it.
add_subdirectory(mimalloc EXCLUDE_FROM_ALL)

# Public wrapper target linked by arangod and the client tools.
#
# For a *static* override of the process allocator the whole archive must be
# pulled in: the linker otherwise only selects the archive members that resolve
# an already-referenced symbol, which can leave part of the override (the C++
# operator new/delete replacements and the initialization constructor) unlinked
# -> a partial override that silently splits allocations between mimalloc and the
# system allocator, invalidating any comparison against jemalloc.
#
# Linking the mimalloc-static target (rather than a hardcoded archive path)
# propagates its public include directory and picks up the build-type dependent
# library name (mimalloc vs. mimalloc-debug) automatically.
# Only the mimalloc archive itself must sit inside the whole-archive region --
# referenced by file, not by target, so mimalloc-static's transitive system
# dependencies (pthread/atomic/rt) are NOT force-loaded. Force-loading them
# breaks static (`-static`) links: e.g. `-latomic` resolves to libatomic.a and
# --whole-archive pulls in members (init.o) that an on-demand -latomic elsewhere
# on the line already provided -> duplicate __libat_* symbols. The plain
# mimalloc-static target is linked afterwards to carry the include directory and
# those transitive deps at a normal (non-whole-archive) position.
add_library(mimalloc INTERFACE)
if (APPLE)
  target_link_libraries(mimalloc INTERFACE
    -Wl,-force_load,$<TARGET_FILE:mimalloc-static> mimalloc-static)
else ()
  target_link_libraries(mimalloc INTERFACE
    -Wl,--whole-archive $<TARGET_FILE:mimalloc-static> -Wl,--no-whole-archive
    mimalloc-static)
endif ()
