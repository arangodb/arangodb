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

#ifndef IRESEARCH_SEGMENT_READER_IMPL_H
#define IRESEARCH_SEGMENT_READER_IMPL_H

#include "index_reader.hpp"
#include "utils/hash_utils.hpp"

NS_ROOT

class segment_reader_impl : public sub_reader {
 public:
  static sub_reader::ptr open(
    const directory& dir, 
    const segment_meta& meta
  );

  const directory& dir() const NOEXCEPT { 
    return dir_;
  }

  virtual index_reader::reader_iterator begin() const override;
  virtual index_reader::reader_iterator end() const override;

  virtual const column_meta* column(const string_ref& name) const override;

  virtual column_iterator::ptr columns() const override;

  virtual uint64_t docs_count() const override {
    return docs_count_;
  }

  virtual docs_iterator_t::ptr docs_iterator() const override;

  virtual const term_reader* field(const string_ref& name) const override {
    return field_reader_->field(name);
  }

  virtual field_iterator::ptr fields() const override {
    return field_reader_->iterator();
  }

  virtual uint64_t live_docs_count() const NOEXCEPT override {
    return docs_count_ - docs_mask_.size();
  }

  uint64_t meta_version() const NOEXCEPT {
    return meta_version_;
  }

  virtual size_t size() const NOEXCEPT override {
    return 1; // only 1 segment
  }

  virtual const columnstore_reader::column_reader* column_reader(
    field_id field
  ) const override;

 private:
  DECLARE_SPTR(segment_reader_impl); // required for NAMED_PTR(...)
  std::vector<column_meta> columns_;
  columnstore_reader::ptr columnstore_reader_;
  const directory& dir_;
  uint64_t docs_count_;
  document_mask docs_mask_;
  field_reader::ptr field_reader_;
  std::vector<column_meta*> id_to_column_;
  uint64_t meta_version_;
  std::unordered_map<hashed_string_ref, column_meta*> name_to_column_;

  segment_reader_impl(
    const directory& dir,
    uint64_t meta_version,
    uint64_t docs_count
  );
};

NS_END

#endif
