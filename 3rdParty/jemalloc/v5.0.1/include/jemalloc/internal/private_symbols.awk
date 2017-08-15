#!/usr/bin/env awk -f

BEGIN {
  sym_prefix = ""
  split("\
        aligned_alloc \
        calloc \
        dallocx \
        free \
        mallctl \
        mallctlbymib \
        mallctlnametomib \
        malloc \
        malloc_conf \
        malloc_message \
        malloc_stats_print \
        malloc_usable_size \
        mallocx \
        nallocx \
        posix_memalign \
        rallocx \
        realloc \
        sallocx \
        sdallocx \
        xallocx \
        memalign \
        valloc \
        __libc_calloc \
        __libc_free \
        __libc_malloc \
        __libc_memalign \
        __libc_realloc \
        __libc_valloc \
        pthread_create \
        __free_hook \
        __malloc_hook \
        __realloc_hook \
        __memalign_hook \
        ", exported_symbol_names)
  # Store exported symbol names as keys in exported_symbols.
  for (i in exported_symbol_names) {
    exported_symbols[exported_symbol_names[i]] = 1
  }
}

# Process 'nm -a <c_source.o>' output.
#
# Handle lines like:
#   0000000000000008 D opt_junk
#   0000000000007574 T malloc_initialized
(NF == 3 && $2 ~ /^[ABCDGRSTVW]$/ && !($3 in exported_symbols) && $3 ~ /^[A-Za-z0-9_]+$/) {
  print substr($3, 1+length(sym_prefix), length($3)-length(sym_prefix))
}

# Process 'dumpbin /SYMBOLS <c_source.obj>' output.
#
# Handle lines like:
#   353 00008098 SECT4  notype       External     | opt_junk
#   3F1 00000000 SECT7  notype ()    External     | malloc_initialized
($3 ~ /^SECT[0-9]+/ && $(NF-2) == "External" && !($NF in exported_symbols)) {
  print $NF
}
