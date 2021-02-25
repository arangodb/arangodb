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

#include "shared.hpp"
#include "mmap_utils.hpp"
#include "utils/log.hpp"

#include <cassert>

namespace iresearch {
namespace mmap_utils {

int flush(int fd, void* addr, size_t size, int flags) noexcept {  
  if (fd < 0) { // an invalid file descriptor of course means an invalid handle
    return 0;
  }

  // sync mmapped region
  if (msync(addr, size, flags)) {
    return -1;
  }

#ifdef __APPLE__
  if (fcntl(fd, F_FULLFSYNC, 0) < 0) {
    return -1;
  }
#endif

#ifdef _MSC_VER
  // under windows all flushes are achieved synchronously, however
  // under windows, there is no guarentee that the underlying disk hardware
  // cache has physically written to disk.
  // FlushFileBuffers ensures file written to disk
  if (((flags & MS_SYNC) == MS_SYNC) && !file_utils::file_sync(fd)) {
    return -1;
  }
#endif
  
  return 0;
}

void mmap_handle::close() noexcept {
  if (addr_ != MAP_FAILED) {
    if (dontneed_) {
      advise(IR_MADVICE_DONTNEED);
    }
    munmap(addr_, size_);
  }
 
  if (fd_ >= 0) {
    ::posix_close(static_cast<int>(fd_));
  }
}

void mmap_handle::init() noexcept {
  fd_ = -1;
  addr_ = MAP_FAILED;
  size_ = 0;
  dontneed_ = false;
}

bool mmap_handle::open(const file_path_t path) noexcept {
  assert(path);

  close();
  init();

  const int fd = ::posix_open(path, O_RDONLY);

  if (fd < 0) {
    IR_FRMT_ERROR("Failed to open input file, error: %d, path: " IR_FILEPATH_SPECIFIER, errno, path);
    close();
    return false;
  }

  fd_ = fd;

  uint64_t size;

  if (!irs::file_utils::byte_size(size, fd)) {
    IR_FRMT_ERROR("Failed to get stats for input file, error: %d, path: " IR_FILEPATH_SPECIFIER, errno, path);
    close();
    return false;
  }

  if (size) {
    size_ = size;

    void* addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (MAP_FAILED == addr) {
      IR_FRMT_ERROR("Failed to mmap input file, error: %d, path: " IR_FILEPATH_SPECIFIER, errno, path);
      close();
      return false;
    }

    addr_ = addr;
  }

  return true;
}

} // mmap_utils
} // ROOT
