//
// IResearch search engine
//
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
//
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
//

#ifndef _MSC_VER
  #include <execinfo.h> // for backtrace(...)
  #include <malloc.h>
#endif

#include "memory.hpp"

NS_ROOT
NS_BEGIN(memory)

void dump_mem_stats_trace() NOEXCEPT {
  #ifndef _MSC_VER
    // ...........................................................................
    // output mallinfo()
    // ...........................................................................
    static const char* fomrat = "\
Total non-mmapped bytes (arena):       %lu\n\
# of free chunks (ordblks):            %lu\n\
# of free fastbin blocks (smblks):     %lu\n\
# of mapped regions (hblks):           %lu\n\
Bytes in mapped regions (hblkhd):      %lu\n\
Max. total allocated space (usmblks):  %lu\n\
Free bytes held in fastbins (fsmblks): %lu\n\
Total allocated space (uordblks):      %lu\n\
Total free space (fordblks):           %lu\n\
Topmost releasable block (keepcost):   %lu\n\
";
    auto mi = mallinfo();

    fprintf(
      stderr,
      fomrat,
      static_cast<unsigned int>(mi.arena),
      static_cast<unsigned int>(mi.ordblks),
      static_cast<unsigned int>(mi.smblks),
      static_cast<unsigned int>(mi.hblks),
      static_cast<unsigned int>(mi.hblkhd),
      static_cast<unsigned int>(mi.usmblks),
      static_cast<unsigned int>(mi.fsmblks),
      static_cast<unsigned int>(mi.uordblks),
      static_cast<unsigned int>(mi.fordblks),
      static_cast<unsigned int>(mi.keepcost)
    );

    // ...........................................................................
    // output malloc_stats()
    // ...........................................................................
    malloc_stats(); // outputs to stderr

    // ...........................................................................
    // output stacktrace
    // ...........................................................................
    static const size_t frames_max = 128; // arbitrary size
    void* frames_buf[frames_max];

    backtrace_symbols_fd(frames_buf, backtrace(frames_buf, frames_max), fileno(stderr));
  #endif
}

NS_END // memory
NS_END