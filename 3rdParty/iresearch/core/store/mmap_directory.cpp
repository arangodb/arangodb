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

#include "mmap_directory.hpp"
#include "store_utils.hpp"
#include "utils/utf8_path.hpp"
#include "utils/mmap_utils.hpp"
#include "utils/memory.hpp"

namespace {

using namespace irs;
using mmap_utils::mmap_handle;

using mmap_handle_ptr = std::shared_ptr<mmap_handle>;

//////////////////////////////////////////////////////////////////////////////
/// @brief converts the specified IOAdvice to corresponding posix madvice
//////////////////////////////////////////////////////////////////////////////
inline int get_posix_madvice(IOAdvice advice) {
  switch (advice) {
    case IOAdvice::NORMAL:
      return IR_MADVICE_NORMAL;
    case IOAdvice::SEQUENTIAL:
      return IR_MADVICE_SEQUENTIAL;
    case IOAdvice::RANDOM:
      return IR_MADVICE_RANDOM;
    case IOAdvice::READONCE:
      return IR_MADVICE_NORMAL;
    case IOAdvice::READONCE_SEQUENTIAL:
      return IR_MADVICE_SEQUENTIAL;
    case IOAdvice::READONCE_RANDOM:
      return IR_MADVICE_RANDOM;
  }

  IR_FRMT_ERROR(
    "madvice '%d' is not valid (RANDOM|SEQUENTIAL), fallback to NORMAL",
    uint32_t(advice)
  );

  return IR_MADVICE_NORMAL;
}

//////////////////////////////////////////////////////////////////////////////
/// @struct mmap_index_input
/// @brief input stream for memory mapped directory
//////////////////////////////////////////////////////////////////////////////
class mmap_index_input : public bytes_ref_input {
 public:
  static index_input::ptr open(
      const file_path_t file,
      IOAdvice advice) noexcept {
    assert(file);

    mmap_handle_ptr handle;

    try {
      handle = memory::make_shared<mmap_handle>();
    } catch (...) {
      return nullptr;
    }

    if (!handle->open(file)) {
      IR_FRMT_ERROR("Failed to open mmapped input file, path: " IR_FILEPATH_SPECIFIER, file);
      return nullptr;
    }

    if (0 == handle->size()) {
      return memory::make_unique<bytes_ref_input>();
    }

    const int padvice = get_posix_madvice(advice);

    if (IR_MADVICE_NORMAL != padvice && !handle->advise(padvice)) {
      IR_FRMT_ERROR("Failed to madvise input file, path: " IR_FILEPATH_SPECIFIER ", error %d", file, errno);
    }

    handle->dontneed(bool(advice & IOAdvice::READONCE));

    try {
      return ptr(new mmap_index_input(std::move(handle)));
    } catch (...) {
      return nullptr;
    }
  }

  virtual ptr dup() const override {
    return ptr(new mmap_index_input(*this));
  }

  virtual ptr reopen() const override {
    return dup();
  }

 private:
  mmap_index_input(mmap_handle_ptr&& handle) noexcept
    : handle_(std::move(handle)) {
    if (handle_) {
      assert(handle_->addr() != MAP_FAILED);
      assert(handle_->size());

      const auto* begin = reinterpret_cast<byte_type*>(handle_->addr());
      bytes_ref_input::reset(begin, handle_->size());
    }
  }

  mmap_index_input(const mmap_index_input& rhs) noexcept
    : bytes_ref_input(rhs),
      handle_(rhs.handle_) {
  }

  mmap_index_input& operator=(const mmap_index_input&) = delete;

  mmap_handle_ptr handle_;
}; // mmap_index_input

} // LOCAL

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                     mmap_directory implementation
// -----------------------------------------------------------------------------

mmap_directory::mmap_directory(const std::string& path)
  : fs_directory(path) {
}

index_input::ptr mmap_directory::open(
    const std::string& name,
    IOAdvice advice) const noexcept {
  utf8_path path;

  try {
    (path/=directory())/=name;
  } catch(...) {
    return nullptr;
  }

  return mmap_index_input::open(path.c_str(), advice);
}

} // ROOT
