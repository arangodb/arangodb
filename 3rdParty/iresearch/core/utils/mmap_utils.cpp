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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "mmap_utils.hpp"
#include "utils/log.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>   // ::close
#include <sys/mman.h> // ::mmap
#include <fcntl.h>    // ::open
#endif // _WIN32

#include <cassert>

NS_ROOT
NS_BEGIN(mmap_utils)

mmap_handle::operator bool() const NOEXCEPT {
#ifdef _WIN32
  // FIXME TODO
  return false;
#else
  return fd_ >= 0;
#endif // _WIN32
}

void mmap_handle::close() NOEXCEPT {
#ifdef _WIN32
  if (NULL != fd_) {
    ::CloseHandle(reinterpret_cast<HANDLE>(fd_));
  }
  // FIXME TODO
#else
  if (addr_ != MAP_FAILED) {
    ::munmap(addr_, size_);
  }

  if (fd_ >= 0) {
    ::close(fd_);
  }
#endif // _WIN32
}

void mmap_handle::init() NOEXCEPT {
#ifdef _WIN32
  fd_ = NULL;
  // FIXME TODO
#else
  addr_ = MAP_FAILED;
  fd_ = -1;
#endif // _WIN32
  size_ = 0;
}

bool mmap_handle::open(const file_path_t path) NOEXCEPT {
  assert(path);

  close();
  init();

#ifdef _WIN32
  // FIXME TODO
  return false;
#else
  const int fd = ::open(path, O_RDONLY);

  if (fd < 0) {
    IR_FRMT_ERROR("Failed to open input file, error: %d, path: %s", errno, path);
    close();
    return false;
  }

  fd_ = fd;

  const auto size = irs::file_utils::file_size(fd);

  if (size < 0) {
    IR_FRMT_ERROR("Failed to get stats for input file, error: %d, path: %s", errno, path);
    close();
    return false;
  }

  if (size) {
    size_ = size;

    void* addr = ::mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (MAP_FAILED == addr) {
      IR_FRMT_ERROR("Failed to mmap input file, error: %d, path: %s", errno, path);
      close();
      return false;
    }

    addr_ = addr;
  }
#endif // _WIN32

  return true;
}

NS_END // mmap_utils
NS_END // ROOT
