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

#ifndef IRESEARCH_SEGMENT_READER_H
#define IRESEARCH_SEGMENT_READER_H

#include "index_reader.hpp"
#include "utils/object_pool.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @brief interface for a segment reader
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API segment_reader final
    : public sub_reader,
      private atomic_shared_ptr_helper<const sub_reader> {
 public:
  typedef atomic_shared_ptr_helper<const sub_reader> atomic_utils;
  typedef segment_reader element_type; // type same as self
  typedef segment_reader ptr; // pointer to self

  template<typename T>
  static bool has(const segment_meta& meta) NOEXCEPT;

  static segment_reader open(const directory& dir, const segment_meta& meta);

  segment_reader() = default; // required for context<segment_reader>
  segment_reader(const segment_reader& other) NOEXCEPT;
  segment_reader& operator=(const segment_reader& other) NOEXCEPT;

  explicit operator bool() const NOEXCEPT { return bool(impl_); }

  bool operator==(const segment_reader& rhs) const NOEXCEPT {
    return impl_ == rhs.impl_;
  }

  bool operator!=(const segment_reader& rhs) const NOEXCEPT {
    return !(*this == rhs);
  }

  segment_reader& operator*() NOEXCEPT { return *this; }
  const segment_reader& operator*() const NOEXCEPT { return *this; }
  segment_reader* operator->() NOEXCEPT { return this; }
  const segment_reader* operator->() const NOEXCEPT { return this; }

  virtual const sub_reader& operator[](size_t i) const NOEXCEPT override {
    assert(!i);
    return *this;
  }

  virtual const column_meta* column(const string_ref& name) const override {
    return impl_->column(name);
  }

  virtual column_iterator::ptr columns() const override {
    return impl_->columns();
  }

  using sub_reader::docs_count;
  virtual uint64_t docs_count() const override {
    return impl_->docs_count();
  }

  virtual doc_iterator::ptr docs_iterator() const override {
    return impl_->docs_iterator();
  }

  // FIXME find a better way to mask documents
  virtual doc_iterator::ptr mask(doc_iterator::ptr&& it) const override {
    return impl_->mask(std::move(it));
  }

  virtual const term_reader* field(const string_ref& name) const override {
    return impl_->field(name);
  }

  virtual field_iterator::ptr fields() const override {
    return impl_->fields();
  }

  virtual uint64_t live_docs_count() const override {
    return impl_->live_docs_count();
  }

  segment_reader reopen(const segment_meta& meta) const;

  void reset() NOEXCEPT {
    impl_.reset();
  }

  virtual size_t size() const override {
    return impl_->size();
  }

  using sub_reader::column_reader;
  virtual const columnstore_reader::column_reader* column_reader(
      field_id field) const override {
    return impl_->column_reader(field);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief converts current 'segment_reader' to 'sub_reader::ptr'
  ////////////////////////////////////////////////////////////////////////////////
  explicit operator sub_reader::ptr() const NOEXCEPT {
    return impl_;
  }

 private:
  typedef std::shared_ptr<const sub_reader> impl_ptr;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  impl_ptr impl_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  segment_reader(impl_ptr&& impl) NOEXCEPT;
}; // segment_reade

template<>
/*static*/ IRESEARCH_API bool segment_reader::has<columnstore_reader>(
    const segment_meta& meta
) NOEXCEPT;

template<>
/*static*/ IRESEARCH_API bool segment_reader::has<document_mask_reader>(
    const segment_meta& meta
) NOEXCEPT;

NS_END

#endif
