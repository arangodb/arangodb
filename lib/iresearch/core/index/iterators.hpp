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

#include "formats/seek_cookie.hpp"
#include "index/index_features.hpp"
#include "shared.hpp"
#include "utils/attribute_provider.hpp"
#include "utils/attributes.hpp"
#include "utils/iterator.hpp"
#include "utils/memory.hpp"
#include "utils/type_limits.hpp"

namespace irs {

// An iterator providing sequential and random access to a posting list
//
// After creation iterator is in uninitialized state:
//   - `value()` returns `doc_limits::invalid()` or `doc_limits::eof()`
// `seek()`, `shallow_seek()` to:
//   - `doc_limits::invalid()` is undefined and implementation dependent
//   - `doc_limits::eof()` must always return `doc_limits::eof()`
// Once iterator is exhausted:
//   - `next()` must constantly return `false`
//   - `seek()`, `shallow_seek()` to any value must return `doc_limits::eof()`
//   - `value()` must return `doc_limits::eof()`
struct doc_iterator : iterator<doc_id_t, attribute_provider> {
  using ptr = memory::managed_ptr<doc_iterator>;

  // Return an empty iterator
  [[nodiscard]] static doc_iterator::ptr empty();

  // Position iterator at a specified target and returns current value
  // (for more information see class description)
  virtual doc_id_t seek(doc_id_t target) = 0;

  // protected:
  // For any doc_iterator we want to define block.
  // It's two bounds: (min...max]:
  // doc_iterator always positioned to the some block.
  //
  // doc_iterator before seek/next/shallow_seek(any valid target)
  // positioned to the first block.
  // You could know about it max, with call shallow_seek(doc_limits::invalid());
  //
  // shallow_seek could move iterator to the some next block.
  // If target is not in the current block or
  // min competitive score force moving to the next block.
  virtual doc_id_t shallow_seek(doc_id_t target) {
    seek(target);
    return doc_limits::eof();
  }
};

// Same as `doc_iterator` but also support `reset()` operation
struct resettable_doc_iterator : doc_iterator {
  // Reset iterator to initial state
  virtual void reset() = 0;
};

struct term_reader;

// An iterator providing sequential and random access to indexed fields
struct field_iterator : iterator<const term_reader&> {
  using ptr = memory::managed_ptr<field_iterator>;

  // Return an empty iterator
  [[nodiscard]] static field_iterator::ptr empty();

  // Position iterator at a specified target.
  // Return if the target is found, false otherwise.
  virtual bool seek(std::string_view target) = 0;
};

struct column_reader;

// An iterator providing sequential and random access to stored columns.
struct column_iterator : iterator<const column_reader&> {
  using ptr = memory::managed_ptr<column_iterator>;

  // Return an empty iterator.
  [[nodiscard]] static column_iterator::ptr empty();

  // Position iterator at a specified target.
  // Return if the target is found, false otherwise.
  virtual bool seek(std::string_view name) = 0;
};

// An iterator providing sequential access to term dictionary
struct term_iterator : iterator<bytes_view, attribute_provider> {
  using ptr = memory::managed_ptr<term_iterator>;

  // Return an empty iterator
  [[nodiscard]] static term_iterator::ptr empty();

  // Read term attributes
  virtual void read() = 0;

  // Return iterator over the associated posting list with the requested
  // features.
  [[nodiscard]] virtual doc_iterator::ptr postings(
    IndexFeatures features) const = 0;
};

// Represents a result of seek operation
enum class SeekResult {
  // Exact value is found
  FOUND = 0,

  // Exact value is not found, an iterator is positioned at the next
  // greater value.
  NOT_FOUND,

  // No value greater than a target found, eof
  END
};

// An iterator providing random and sequential access to term
// dictionary.
struct seek_term_iterator : term_iterator {
  using ptr = memory::managed_ptr<seek_term_iterator>;

  // Return an empty iterator
  [[nodiscard]] static seek_term_iterator::ptr empty();

  // Position iterator at a value that is not less than the specified
  // one. Returns seek result.
  virtual SeekResult seek_ge(bytes_view value) = 0;

  // Position iterator at a value that is not less than the specified
  // one. Returns `true` on success, `false` otherwise.
  // Caller isn't allowed to read iterator value in case if this method
  // returned `false`.
  virtual bool seek(bytes_view value) = 0;

  // Returns seek cookie of the current term value.
  [[nodiscard]] virtual seek_cookie::ptr cookie() const = 0;
};

// Position iterator to the specified target and returns current value
// of the iterator. Returns `false` if iterator exhausted, `true` otherwise.
template<typename Iterator, typename T, typename Less = std::less<T>>
bool seek(Iterator& it, const T& target, Less less = Less()) {
  bool next = true;
  while (less(it.value(), target) && true == (next = it.next()))
    ;
  return next;
}

// Position iterator to the specified min term or to the next term
// after the min term depending on the specified `Include` value.
// Returns true in case if iterator has been successfully positioned,
// false otherwise.
template<bool Include>
bool seek_min(seek_term_iterator& it, bytes_view min) {
  const auto res = it.seek_ge(min);

  return SeekResult::END != res &&
         (Include || SeekResult::FOUND != res || it.next());
}

// Position iterator `count` items after the current position.
// Returns true if the iterator has been successfully positioned
template<typename Iterator>
bool skip(Iterator& itr, size_t count) {
  while (count--) {
    if (!itr.next()) {
      return false;
    }
  }

  return true;
}

}  // namespace irs
