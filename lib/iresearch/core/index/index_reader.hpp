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

#pragma once

#include "formats/formats.hpp"
#include "index/index_reader_options.hpp"
#include "utils/iterator.hpp"

namespace irs {

struct SubReader;
struct SegmentMeta;

// Generic interface for accessing an index
struct IndexReader {
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = const SubReader;
    using pointer = value_type*;
    using reference = value_type&;
    using difference_type = ptrdiff_t;

    reference operator*() const {
      // can't mark noexcept because of virtual operator[]
      IRS_ASSERT(i_ < reader_->size());
      return (*reader_)[i_];
    }

    pointer operator->() const { return &(**this); }

    Iterator& operator++() noexcept {
      ++i_;
      return *this;
    }

    Iterator operator++(int) noexcept {
      auto it = *this;
      ++(*this);
      return it;
    }

    bool operator==(const Iterator& rhs) const noexcept {
      IRS_ASSERT(reader_ == rhs.reader_);
      return i_ == rhs.i_;
    }

    difference_type operator-(const Iterator& rhs) const noexcept {
      IRS_ASSERT(reader_ == rhs.reader_);
      return i_ - rhs.i_;
    }

   private:
    friend struct IndexReader;

    Iterator(const IndexReader& reader, size_t i) noexcept
      : reader_{&reader}, i_{i} {}

    const IndexReader* reader_;
    size_t i_;
  };

  using ptr = std::shared_ptr<const IndexReader>;

  virtual ~IndexReader() = default;

  // count memory
  virtual uint64_t CountMappedMemory() const = 0;

  // number of live documents
  virtual uint64_t live_docs_count() const = 0;

  // total number of documents including deleted
  virtual uint64_t docs_count() const = 0;

  // return the i'th sub_reader
  virtual const SubReader& operator[](size_t i) const = 0;

  // returns number of sub-segments in current reader
  virtual size_t size() const = 0;

  // first sub-segment
  Iterator begin() const noexcept { return Iterator{*this, 0}; }

  // after the last sub-segment
  Iterator end() const { return Iterator{*this, size()}; }
};

struct ColumnProvider {
  virtual ~ColumnProvider() = default;

  virtual const irs::column_reader* column(field_id field) const = 0;

  virtual const irs::column_reader* column(std::string_view field) const = 0;
};

// Generic interface for accessing an index segment
struct SubReader : public IndexReader, public ColumnProvider {
  using ptr = std::shared_ptr<const SubReader>;

  static const SubReader& empty() noexcept;

  uint64_t live_docs_count() const final { return Meta().live_docs_count; }

  uint64_t docs_count() const final { return Meta().docs_count; }

  const SubReader& operator[](size_t i) const noexcept final {
    IRS_ASSERT(i == 0);
    IRS_IGNORE(i);
    return *this;
  }

  size_t size() const noexcept final { return 1; }

  virtual const SegmentInfo& Meta() const = 0;

  // Live & deleted docs

  virtual const DocumentMask* docs_mask() const = 0;

  // Returns an iterator over live documents in current segment.
  virtual doc_iterator::ptr docs_iterator() const = 0;

  virtual doc_iterator::ptr mask(doc_iterator::ptr&& it) const {
    return std::move(it);
  }

  // Inverted index

  virtual field_iterator::ptr fields() const = 0;

  // Returns corresponding term_reader by the specified field name.
  virtual const term_reader* field(std::string_view field) const = 0;

  // Columnstore

  virtual column_iterator::ptr columns() const = 0;

  virtual const irs::column_reader* sort() const = 0;
};

template<typename Visitor, typename FilterVisitor>
void visit(const IndexReader& index, std::string_view field,
           const FilterVisitor& field_visitor, Visitor& visitor) {
  for (auto& segment : index) {
    const auto* reader = segment.field(field);

    if (IRS_LIKELY(reader)) {
      field_visitor(segment, *reader, visitor);
    }
  }
}

}  // namespace irs
