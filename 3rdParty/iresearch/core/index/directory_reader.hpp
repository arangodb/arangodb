//
// IResearch search engine 
//
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
//
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
//

#ifndef IRESEARCH_DIRECTORY_READER_H
#define IRESEARCH_DIRECTORY_READER_H

#include "shared.hpp"
#include "index_reader.hpp"
#include "utils/object_pool.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @brief interface for an index reader over a directory of segments
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API directory_reader final
    : public composite_reader,
      private atomic_base<std::shared_ptr<composite_reader>> {
 public:
  typedef atomic_base<std::shared_ptr<composite_reader>> atomic_utils;
  typedef directory_reader element_type; // type same as self
  typedef directory_reader ptr; // pointer to self

  directory_reader() = default; // allow creation of an uninitialized ptr
  directory_reader(const directory_reader& other) NOEXCEPT;
  directory_reader& operator=(const directory_reader& other) NOEXCEPT;

  explicit operator bool() const NOEXCEPT { return bool(impl_); }

  directory_reader& operator*() NOEXCEPT { return *this; }
  const directory_reader& operator*() const NOEXCEPT { return *this; }
  directory_reader* operator->() NOEXCEPT { return this; }
  const directory_reader* operator->() const NOEXCEPT { return this; }

  virtual const sub_reader& operator[](size_t i) const override {
    return (*impl_)[i];
  }

  virtual doc_id_t base(size_t i) const override {
    return impl_->base(i);
  }

  virtual index_reader::reader_iterator begin() const override {
    return impl_->begin();
  }

  virtual index_reader::reader_iterator end() const override {
    return impl_->end();
  }

  virtual uint64_t docs_count() const override {
    return impl_->docs_count();
  }

  virtual uint64_t docs_count(const string_ref& field) const override {
    return impl_->docs_count(field);
  }

  virtual uint64_t live_docs_count() const override {
    return impl_->live_docs_count();
  }

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

  void reset() NOEXCEPT {
    impl_.reset();
  }

 private:
  typedef std::shared_ptr<composite_reader> impl_ptr;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  impl_ptr impl_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  directory_reader(impl_ptr&& impl) NOEXCEPT;
}; // directory_reader

NS_END

#endif
