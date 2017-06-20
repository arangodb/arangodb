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
#include "segment_reader.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @brief interface for an index reader over a directory of segments
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API directory_reader: public composite_reader {
 public:
  typedef directory_reader element_type; // type same as self
  typedef directory_reader ptr; // pointer to self
  directory_reader() = default; // allow creation of an uninitialized ptr
  directory_reader(const directory_reader& other);
  directory_reader(directory_reader&& other) NOEXCEPT;
  directory_reader& operator=(const directory_reader& other);
  directory_reader& operator=(directory_reader&& other) NOEXCEPT;
  explicit operator bool() const NOEXCEPT;
  directory_reader& operator*() NOEXCEPT;
  const directory_reader& operator*() const NOEXCEPT;
  directory_reader* operator->() NOEXCEPT;
  const directory_reader* operator->() const NOEXCEPT;
  virtual const sub_reader& operator[](size_t i) const override;

  virtual doc_id_t base(size_t i) const override;
  virtual index_reader::reader_iterator begin() const override;
  virtual uint64_t docs_count() const override;
  virtual uint64_t docs_count(const string_ref& field) const override;
  virtual index_reader::reader_iterator end() const override;
  virtual uint64_t live_docs_count() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create an index reader over the specified directory
  ///        if codec == nullptr then use the latest file for all known codecs
  ////////////////////////////////////////////////////////////////////////////////
  static directory_reader open(
    const directory& dir, const format::ptr& codec = format::ptr(nullptr)
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief open a new instance based on the latest file for the specified codec
  ///        this call will atempt to reuse segments from the existing reader
  ///        if codec == nullptr then use the latest file for all known codecs
  ////////////////////////////////////////////////////////////////////////////////
  virtual directory_reader reopen(
    const format::ptr& codec = format::ptr(nullptr)
  ) const;

  void reset() NOEXCEPT;
  virtual size_t size() const override;

 private:
  class atomic_helper;
  class directory_reader_impl;
  typedef std::shared_ptr<directory_reader_impl> impl_ptr;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  impl_ptr impl_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  directory_reader(const impl_ptr& impl);
};

NS_END

#endif