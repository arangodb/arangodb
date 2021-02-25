////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_MMAP_UTILS_H
#define IRESEARCH_MMAP_UTILS_H

#include "file_utils.hpp"
#include "noncopyable.hpp"

#if defined(_MSC_VER)

#include "mman_win32.hpp"

////////////////////////////////////////////////////////////////////////////////
/// @brief constants for madvice
////////////////////////////////////////////////////////////////////////////////
#define IR_MADVICE_NORMAL 0
#define IR_MADVICE_SEQUENTIAL 0
#define IR_MADVICE_RANDOM 0
#define IR_MADVICE_WILLNEED 0
#define IR_MADVICE_DONTNEED 0
#define IR_MADVICE_DONTDUMP 0

#else

#include <sys/mman.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief create a wrapper for MAP_ANONYMOUS / MAP_ANON
///
///               MAP_ANON  MAP_ANONYMOUS
///    OpenBSD         Y         Y(*)
///    Linux           Y(*)      Y
///    FreeBSD         Y         Y
///    NetBSD          Y         N
///    OSX             Y         N
///    Solaris         Y         N
///    HP-UX           N         Y
///    AIX             N         Y
///    IRIX            N         N
///    (*) synonym to other
///
////////////////////////////////////////////////////////////////////////////////
#ifndef MAP_ANON
#ifdef MAP_ANONYMOUS
#define MAP_ANON MAP_ANONYMOUS
#else
#error "System does not support mapping anonymous pages?"
#endif
#endif // MAP_ANON

////////////////////////////////////////////////////////////////////////////////
/// @brief constants for madvice
////////////////////////////////////////////////////////////////////////////////
#define IR_MADVICE_NORMAL MADV_NORMAL
#define IR_MADVICE_SEQUENTIAL MADV_SEQUENTIAL
#define IR_MADVICE_RANDOM MADV_RANDOM
#define IR_MADVICE_WILLNEED MADV_WILLNEED
#define IR_MADVICE_DONTNEED MADV_DONTNEED

#endif // _MSC_VER

namespace iresearch {
namespace mmap_utils {

//////////////////////////////////////////////////////////////////////////////
/// @brief flushes changes made in memory back to disk
//////////////////////////////////////////////////////////////////////////////
int flush(int fd, void* addr, size_t size, int flags) noexcept;

//////////////////////////////////////////////////////////////////////////////
/// @class mmap_handle
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API mmap_handle : private util::noncopyable {
 public:
  mmap_handle() noexcept {
    init();
  }

  ~mmap_handle() noexcept {
    close();
  }

  bool open(const file_path_t file) noexcept;
  void close() noexcept;

  explicit operator bool() const noexcept {
    return fd_ >= 0;
  }

  void* addr() const noexcept { return addr_; }
  size_t size() const noexcept { return size_; }
  ptrdiff_t fd() const noexcept { return fd_; }

  bool flush(int flags) noexcept {
    return !mmap_utils::flush(
      static_cast<int>(fd_), addr_, size_, flags
    );
  }

  bool advise(int advice) noexcept {
    return 0 == ::madvise(addr_, size_, advice);
  }

  void dontneed(bool value) noexcept {
    dontneed_ = value;
  }

 private:
  void init() noexcept;

  void* addr_; // the beginning of mmapped region
  size_t size_; // file size
  ptrdiff_t fd_; // file descriptor
  bool dontneed_; // request to free pages on close
}; // mmap_handle

} // mmap_utils
} // ROOT

#endif // IRESEARCH_MMAP_UTILS_H
