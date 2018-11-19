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

#include "shared.hpp"
#include "composite_reader_impl.hpp"
#include "field_meta.hpp"
#include "index_reader.hpp"
#include "segment_reader.hpp"
#include "index_meta.hpp"
#include "utils/directory_utils.hpp"
#include "utils/type_limits.hpp"
#include "utils/singleton.hpp"

NS_LOCAL

struct empty_sub_reader final : irs::singleton<empty_sub_reader>, irs::sub_reader {
  virtual const irs::column_meta* column(const irs::string_ref& name) const override {
    return nullptr;
  }
  virtual irs::column_iterator::ptr columns() const override {
    return irs::column_iterator::empty();
  }
  virtual const irs::columnstore_reader::column_reader* column_reader(irs::field_id field) const override {
    return nullptr;
  }
  virtual uint64_t docs_count() const override {
    return 0;
  }
  virtual irs::doc_iterator::ptr docs_iterator() const override {
    return irs::doc_iterator::empty();
  }
  virtual const irs::term_reader* field(const irs::string_ref& field) const override {
    return nullptr;
  }
  virtual irs::field_iterator::ptr fields() const override {
    return irs::field_iterator::empty();
  }
  virtual uint64_t live_docs_count() const override {
    return 0;
  }
  virtual const irs::sub_reader& operator[](size_t) const override {
    throw std::out_of_range("index out of range");
  }
  virtual size_t size() const override { return 0; }
}; // index_reader

NS_END // LOCAL

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                       index_reader implementation
// -----------------------------------------------------------------------------

index_reader::~index_reader() { }

// -----------------------------------------------------------------------------
// --SECTION--                                         sub_reader implementation
// -----------------------------------------------------------------------------

/*static*/ const sub_reader& sub_reader::empty() NOEXCEPT {
  return empty_sub_reader::instance();
}

const columnstore_reader::column_reader* sub_reader::column_reader(
    const string_ref& field) const {
  const auto* meta = column(field);
  return meta ? column_reader(meta->id) : nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                             context specialization for sub_reader
// -----------------------------------------------------------------------------

template<>
struct context<sub_reader> {
  sub_reader::ptr reader;
  doc_id_t base = 0; // min document id
  doc_id_t max = 0; // max document id

  operator doc_id_t() const { return max; }
}; // reader_context

NS_END
