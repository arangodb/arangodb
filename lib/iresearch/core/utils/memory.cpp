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
#ifndef DISABLE_EXECINFO
#include <execinfo.h>  // for backtrace(...)
#endif

#include <cstddef>  // needed for __GLIBC__

#if !defined(__APPLE__) && defined(__GLIBC__)
#include <malloc.h>
#endif
#endif

#include "memory.hpp"

namespace irs::memory {

void dump_mem_stats_trace() noexcept {
#ifndef _MSC_VER

// MacOS does not have malloc.h and hence no mallinfo() or malloc_stats()
// libmusl does no define mallinfo() or malloc_stats() in malloc.h
// enable mallinfo() and malloc_stats() for GLIBC only
#if !defined(__APPLE__) && defined(__GLIBC__)
  // output mallinfo()
  constexpr const char* format =
    "\
Total non-mmapped bytes (arena):       %u\n\
# of free chunks (ordblks):            %u\n\
# of free fastbin blocks (smblks):     %u\n\
# of mapped regions (hblks):           %u\n\
Bytes in mapped regions (hblkhd):      %u\n\
Max. total allocated space (usmblks):  %u\n\
Free bytes held in fastbins (fsmblks): %u\n\
Total allocated space (uordblks):      %u\n\
Total free space (fordblks):           %u\n\
Topmost releasable block (keepcost):   %u\n\
";
  auto mi = mallinfo();

  fprintf(stderr, format, static_cast<unsigned>(mi.arena),
          static_cast<unsigned>(mi.ordblks), static_cast<unsigned>(mi.smblks),
          static_cast<unsigned>(mi.hblks), static_cast<unsigned>(mi.hblkhd),
          static_cast<unsigned>(mi.usmblks), static_cast<unsigned>(mi.fsmblks),
          static_cast<unsigned>(mi.uordblks),
          static_cast<unsigned>(mi.fordblks),
          static_cast<unsigned>(mi.keepcost));

  malloc_stats();  // output malloc_stats to stderr
#endif
#ifndef DISABLE_EXECINFO
  // output stacktrace
  void* frames_buf[128];  // arbitrary size

  backtrace_symbols_fd(frames_buf, backtrace(frames_buf, std::size(frames_buf)),
                       fileno(stderr));
#endif
#endif
}

}  // namespace irs::memory
