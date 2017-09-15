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

#ifndef IRESEARCH_MMAP_UTILS_H
#define IRESEARCH_MMAP_UTILS_H

#include "file_utils.hpp"

NS_ROOT
NS_BEGIN(mmap_utils)

//////////////////////////////////////////////////////////////////////////////
/// @class mmap_handle
//////////////////////////////////////////////////////////////////////////////
class mmap_handle {
 public:
  mmap_handle() NOEXCEPT {
    init();
  }

  ~mmap_handle() {
    close();
  }

  bool open(const file_path_t file) NOEXCEPT;
  void close() NOEXCEPT;
  explicit operator bool() const NOEXCEPT;

  void* addr() const NOEXCEPT { return addr_; }
  size_t size() const NOEXCEPT { return size_; }
  ptrdiff_t fd() const NOEXCEPT { return fd_; }

 private:
  void init() NOEXCEPT;

  void* addr_; // the beginning of mmapped region
  size_t size_; // file size
  ptrdiff_t fd_; // file descriptor
}; // mmap_handle

NS_END // mmap_utils
NS_END // ROOT

#endif // IRESEARCH_MMAP_UTILS_H
