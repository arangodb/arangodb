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

#include "utils/mmap_utils.hpp"

#include "file_utils.hpp"
#include "shared.hpp"
#include "utils/assert.hpp"
#include "utils/log.hpp"

#include <absl/strings/str_cat.h>

namespace irs::mmap_utils {

void mmap_handle::close() noexcept {
  if (addr_ != MAP_FAILED) {
    if (dontneed_) {
      advise(IR_MADVICE_DONTNEED);
    }
    munmap(addr_, size_);
  }

  if (fd_ >= 0) {
    ::posix_close(static_cast<int>(fd_));
    rm_.Decrease(1);
  }
}

void mmap_handle::init() noexcept {
  fd_ = -1;
  addr_ = MAP_FAILED;
  size_ = 0;
  dontneed_ = false;
}

bool mmap_handle::open(const path_char_t* path) noexcept try {
  IRS_ASSERT(path);

  close();
  init();

  rm_.Increase(1);
  const int fd = ::posix_open(path, O_RDONLY);
  if (fd < 0) {
    IRS_LOG_ERROR(absl::StrCat("Failed to open input file, error: ", errno,
                               ", path: ", file_utils::ToStr(path)));
    rm_.Decrease(1);
    return false;
  }

  fd_ = fd;

  uint64_t size;

  if (!file_utils::byte_size(size, fd)) {
    IRS_LOG_ERROR(absl::StrCat("Failed to get stats for input file, error: ",
                               errno, ", path: ", file_utils::ToStr(path)));
    close();
    return false;
  }

  if (size) {
    size_ = size;

    // TODO(MBkkt) Needs benchmark?
    //  1. MAP_SHARED can make more sense than MAP_PRIVATE
    //     both ok for us, because file is read only
    //     but with it we probably can avoid COW kernel overhead.
    //  2. MAP_POPULATE | MAP_LOCKED instead of read to vector stuff?
    void* addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (MAP_FAILED == addr) {
      IRS_LOG_ERROR(absl::StrCat("Failed to mmap input file, error: ", errno,
                                 ", path: ", file_utils::ToStr(path)));
      close();
      return false;
    }

    addr_ = addr;
  }

  return true;
} catch (...) {
  return false;
}

}  // namespace irs::mmap_utils
