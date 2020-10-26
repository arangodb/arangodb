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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_INDEX_READER_H
#define IRESEARCH_INDEX_READER_H

#include <vector>
#include <numeric>
#include <functional>

#include "index/field_meta.hpp"
#include "formats/formats.hpp"
#include "store/directory.hpp"
#include "store/directory_attributes.hpp"
#include "utils/iterator.hpp"
#include "utils/memory.hpp"
#include "utils/string.hpp"

namespace iresearch {

struct sub_reader;

////////////////////////////////////////////////////////////////////////////////
/// @struct index_reader
/// @brief generic interface for accessing an index
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API index_reader {
  class reader_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = const sub_reader;
    using pointer = value_type*;
    using reference = value_type&;
    using difference_type = void;

    reference operator*() const {
      // can't mark noexcept because of virtual operator[]
      assert(i_ < reader_->size());
      return (*reader_)[i_];
    }

    pointer operator->() const {
      return &(**this);
    }

    reader_iterator& operator++() noexcept {
      ++i_;
      return *this;
    }

    reader_iterator operator++(int) noexcept {
      auto it = *this;
      ++(*this);
      return it;
    }

    bool operator==(const reader_iterator& rhs) const noexcept {
      assert(reader_ == rhs.reader_);
      return i_ == rhs.i_;
    }

    bool operator!=(const reader_iterator& rhs) const noexcept {
      return !(*this == rhs);
    }

   private:
    friend struct index_reader;

    explicit reader_iterator(const index_reader& reader, size_t i = 0) noexcept
      : reader_(&reader), i_(i) {
    }

    const index_reader* reader_;
    size_t i_;
  }; // reader_iterator

  using ptr = std::shared_ptr<const index_reader>;

  virtual ~index_reader() = default;

  // number of live documents
  virtual uint64_t live_docs_count() const = 0;

  // total number of documents including deleted
  virtual uint64_t docs_count() const = 0;

  // return the i'th sub_reader
  virtual const sub_reader& operator[](size_t i) const = 0;

  // returns number of sub-segments in current reader
  virtual size_t size() const = 0;

  // first sub-segment
  reader_iterator begin() const noexcept {
    return reader_iterator(*this, 0);
  }

  // after the last sub-segment
  reader_iterator end() const {
    return reader_iterator(*this, size());
  }
}; // index_reader

////////////////////////////////////////////////////////////////////////////////
/// @struct sub_reader
/// @brief generic interface for accessing an index segment
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API sub_reader : index_reader {
  using ptr = std::shared_ptr<const sub_reader>;

  static const sub_reader& empty() noexcept;

  // returns iterator over the live documents in current segment
  virtual doc_iterator::ptr docs_iterator() const = 0;

  virtual field_iterator::ptr fields() const = 0;

  virtual doc_iterator::ptr mask(doc_iterator::ptr&& it) const {
    return std::move(it);
  }

  // returns corresponding term_reader by the specified field
  virtual const term_reader* field(const string_ref& field) const = 0;

  virtual column_iterator::ptr columns() const = 0;

  virtual const column_meta* column(const string_ref& name) const = 0;

  virtual const columnstore_reader::column_reader* sort() const = 0;

  virtual const columnstore_reader::column_reader* column_reader(field_id field) const = 0;

  const columnstore_reader::column_reader* column_reader(const string_ref& field) const;
}; // sub_reader

template<typename Visitor, typename FilterVisitor>
void visit(const index_reader& index, const string_ref& field,
           const flags& required, const FilterVisitor& field_visitor,
           Visitor& visitor) {
  for (auto& segment : index) {
    const auto* reader = segment.field(field);

    if (!reader || (!required.empty() && !required.is_subset_of(reader->meta().features))) {
      continue;
    }

    field_visitor(segment, *reader, visitor);
  }
}

template<typename Visitor, typename FilterVisitor>
void visit(const index_reader& index, const string_ref& field,
           const FilterVisitor& field_visitor, Visitor& visitor) {
  visit(index, field, flags::empty_instance(), field_visitor, visitor);
}

}

MSVC_ONLY(template class IRESEARCH_API std::function<bool(iresearch::doc_id_t)>;) // sub_reader::value_visitor_f

#endif
