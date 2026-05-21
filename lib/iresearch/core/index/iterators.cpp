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

#include "index/iterators.hpp"

#include "analysis/token_attributes.hpp"
#include "formats/empty_term_reader.hpp"
#include "index/field_meta.hpp"
#include "search/cost.hpp"
#include "utils/singleton.hpp"
#include "utils/type_limits.hpp"

namespace irs {
namespace {

// Represents an iterator with no documents
struct EmptyDocIterator : doc_iterator {
  doc_id_t value() const final { return doc_limits::eof(); }
  bool next() final { return false; }
  doc_id_t seek(doc_id_t /*target*/) final { return doc_limits::eof(); }
  attribute* get_mutable(type_info::type_id id) noexcept final {
    if (type<document>::id() == id) {
      return &_doc;
    }

    return type<cost>::id() == id ? &_cost : nullptr;
  }

 private:
  cost _cost{0};
  document _doc{doc_limits::eof()};
};

EmptyDocIterator kEmptyDocIterator;

// Represents an iterator without terms
struct EmptyTermIterator : term_iterator {
  bytes_view value() const noexcept final { return {}; }
  doc_iterator::ptr postings(IndexFeatures /*features*/) const noexcept final {
    return doc_iterator::empty();
  }
  void read() noexcept final {}
  bool next() noexcept final { return false; }
  attribute* get_mutable(type_info::type_id /*type*/) noexcept final {
    return nullptr;
  }
};

EmptyTermIterator kEmptyTermIterator;

// Represents an iterator without terms
struct EmptySeekTermIterator : seek_term_iterator {
  bytes_view value() const noexcept final { return {}; }
  doc_iterator::ptr postings(IndexFeatures /*features*/) const noexcept final {
    return doc_iterator::empty();
  }
  void read() noexcept final {}
  bool next() noexcept final { return false; }
  attribute* get_mutable(type_info::type_id /*type*/) noexcept final {
    return nullptr;
  }
  SeekResult seek_ge(bytes_view /*value*/) noexcept final {
    return SeekResult::END;
  }
  bool seek(bytes_view /*value*/) noexcept final { return false; }
  seek_cookie::ptr cookie() const noexcept final { return nullptr; }
};

EmptySeekTermIterator kEmptySeekIterator;

// Represents a reader with no terms
const empty_term_reader kEmptyTermReader{0};

// Represents a reader with no fields
struct EmptyFieldIterator : field_iterator {
  const term_reader& value() const final { return kEmptyTermReader; }

  bool seek(std::string_view /*target*/) final { return false; }

  bool next() final { return false; }
};

EmptyFieldIterator kEmptyFieldIterator;

struct EmptyColumnReader final : column_reader {
  field_id id() const final { return field_limits::invalid(); }

  // Returns optional column name.
  std::string_view name() const final { return {}; }

  // Returns column header.
  bytes_view payload() const final { return {}; }

  // Returns the corresponding column iterator.
  // If the column implementation supports document payloads then it
  // can be accessed via the 'payload' attribute.
  doc_iterator::ptr iterator(ColumnHint /*hint*/) const final {
    return doc_iterator::empty();
  }

  doc_id_t size() const final { return 0; }
};

const EmptyColumnReader kEmptyColumnReader;

// Represents a reader with no columns
struct EmptyColumnIterator : column_iterator {
  const column_reader& value() const final { return kEmptyColumnReader; }

  bool seek(std::string_view /*name*/) final { return false; }

  bool next() final { return false; }
};

EmptyColumnIterator kEmptyColumnIterator;

}  // namespace

term_iterator::ptr term_iterator::empty() {
  return memory::to_managed<term_iterator>(kEmptyTermIterator);
}

seek_term_iterator::ptr seek_term_iterator::empty() {
  return memory::to_managed<seek_term_iterator>(kEmptySeekIterator);
}

doc_iterator::ptr doc_iterator::empty() {
  return memory::to_managed<doc_iterator>(kEmptyDocIterator);
}

field_iterator::ptr field_iterator::empty() {
  return memory::to_managed<field_iterator>(kEmptyFieldIterator);
}

column_iterator::ptr column_iterator::empty() {
  return memory::to_managed<column_iterator>(kEmptyColumnIterator);
}

}  // namespace irs
