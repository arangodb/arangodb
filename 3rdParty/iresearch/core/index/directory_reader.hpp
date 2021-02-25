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

#ifndef IRESEARCH_DIRECTORY_READER_H
#define IRESEARCH_DIRECTORY_READER_H

#include "shared.hpp"
#include "index_reader.hpp"
#include "utils/object_pool.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief representation of the metadata of a directory_reader
////////////////////////////////////////////////////////////////////////////////
struct directory_meta {
  std::string filename;
  index_meta meta;
};

////////////////////////////////////////////////////////////////////////////////
/// @class directory_reader
/// @brief interface for an index reader over a directory of segments
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API directory_reader final
    : public index_reader,
      private atomic_shared_ptr_helper<const index_reader> {
 public:
  typedef atomic_shared_ptr_helper<const index_reader> atomic_utils;
  typedef directory_reader element_type; // type same as self
  typedef directory_reader ptr; // pointer to self

  directory_reader() = default; // allow creation of an uninitialized ptr
  directory_reader(const directory_reader& other) noexcept;
  directory_reader& operator=(const directory_reader& other) noexcept;

  explicit operator bool() const noexcept { return bool(impl_); }

  bool operator==(std::nullptr_t) const noexcept {
    return !impl_;
  }

  bool operator!=(std::nullptr_t) const noexcept {
    return !(*this == nullptr);
  }

  bool operator==(const directory_reader& rhs) const noexcept {
    return impl_ == rhs.impl_;
  }

  bool operator!=(const directory_reader& rhs) const noexcept {
    return !(*this == rhs);
  }

  directory_reader& operator*() noexcept { return *this; }
  const directory_reader& operator*() const noexcept { return *this; }
  directory_reader* operator->() noexcept { return this; }
  const directory_reader* operator->() const noexcept { return this; }

  virtual const sub_reader& operator[](size_t i) const override {
    return (*impl_)[i];
  }

  virtual uint64_t docs_count() const override {
    return impl_->docs_count();
  }

  virtual uint64_t live_docs_count() const override {
    return impl_->live_docs_count();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return the directory_meta this reader is based upon
  /// @note return value valid on an already open reader until call to reopen()
  //////////////////////////////////////////////////////////////////////////////
  const directory_meta& meta() const;

  virtual size_t size() const override {
    return impl_->size();
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create an index reader over the specified directory
  ///        if codec == nullptr then use the latest file for all known codecs
  ////////////////////////////////////////////////////////////////////////////////
  static directory_reader open(
    const directory& dir,
    format::ptr codec = nullptr
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief open a new instance based on the latest file for the specified codec
  ///        this call will atempt to reuse segments from the existing reader
  ///        if codec == nullptr then use the latest file for all known codecs
  ////////////////////////////////////////////////////////////////////////////////
  virtual directory_reader reopen(
    format::ptr codec = nullptr
  ) const;

  void reset() noexcept {
    impl_.reset();
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief converts current directory_reader to 'index_reader::ptr'
  ////////////////////////////////////////////////////////////////////////////////
  explicit operator index_reader::ptr() const noexcept {
    return impl_;
  }

 private:
  typedef std::shared_ptr<const index_reader> impl_ptr;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  impl_ptr impl_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  directory_reader(impl_ptr&& impl) noexcept;
}; // directory_reader

inline bool operator==(std::nullptr_t, const directory_reader& rhs) noexcept {
  return rhs == nullptr;
}

inline bool operator!=(std::nullptr_t, const directory_reader& rhs) noexcept {
  return rhs != nullptr;
}

}

#endif
