////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef _MSC_VER
  #include <execinfo.h> // for backtrace(...)

  #if !defined(__APPLE__) && defined(__GLIBC__)
    #include <malloc.h>
  #endif
#endif

#include "memory.hpp"

namespace iresearch {
namespace memory {

void dump_mem_stats_trace() noexcept {
  #ifndef _MSC_VER

  // MacOS does not have malloc.h and hence no mallinfo() or malloc_stats()
  // libmusl does no define mallinfo() or malloc_stats() in malloc.h
  // enable mallinfo() and malloc_stats() for GLIBC only
  #if !defined(__APPLE__) && defined(__GLIBC__)
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
  #endif

    // ...........................................................................
    // output stacktrace
    // ...........................................................................
    static const size_t frames_max = 128; // arbitrary size
    void* frames_buf[frames_max];

    backtrace_symbols_fd(frames_buf, backtrace(frames_buf, frames_max), fileno(stderr));
  #endif
}

} // memory
}
