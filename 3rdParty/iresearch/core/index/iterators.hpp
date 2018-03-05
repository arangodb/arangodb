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

#ifndef IRESEARCH_ITERATORS_H
#define IRESEARCH_ITERATORS_H

#include "shared.hpp"
#include "utils/attributes.hpp"
#include "utils/attributes_provider.hpp"
#include "utils/iterator.hpp"
#include "utils/integer.hpp"
#include "utils/memory.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                    doc iterators 
// ----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator 
/// @brief base iterator for document collections. 
/// Before the first "next()" iterator is uninitialized. In this case "value()"
/// should return INVALID_DOC.
/// Once iterator has finished execution it should return "false" on last 
/// "next()" call. After that "value()" should always return NO_MORE_DOCS.
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API doc_iterator
    : iterator<doc_id_t>,
      util::const_attribute_view_provider {
  DECLARE_SPTR(doc_iterator);
  DECLARE_FACTORY(doc_iterator);

  static doc_iterator::ptr empty();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief jumps to the specified target and returns current value
  /// of the iterator.
  /// If INVALID_DOC will be passed as the parameter,
  /// return value is not defined
  /// If NO_MORE_DOCS will be passed as the parameter, method should
  /// return NO_MORE_DOCS
  //////////////////////////////////////////////////////////////////////////////
  virtual doc_id_t seek(doc_id_t target) = 0;
}; // doc_iterator

// ----------------------------------------------------------------------------
// --SECTION--                                                  field iterators 
// ----------------------------------------------------------------------------

struct term_reader;

struct IRESEARCH_API field_iterator : iterator<const term_reader&> {
  DECLARE_MANAGED_PTR(field_iterator);

  virtual bool seek(const string_ref& name) = 0;

  static field_iterator::ptr empty();
};

// ----------------------------------------------------------------------------
// --SECTION--                                                 column iterators
// ----------------------------------------------------------------------------

struct column_meta;

struct IRESEARCH_API column_iterator : iterator<const column_meta&> {
  DECLARE_MANAGED_PTR(column_iterator);

  virtual bool seek(const string_ref& name) = 0;
  
  static column_iterator::ptr empty();
};

// ----------------------------------------------------------------------------
// --SECTION--                                                   term iterators 
// ----------------------------------------------------------------------------

struct IRESEARCH_API term_iterator
    : iterator<const bytes_ref&>,
      public util::const_attribute_view_provider {
  DECLARE_MANAGED_PTR(term_iterator);

  static term_iterator::ptr empty();

  /* read attributes */
  virtual void read() = 0;
  virtual doc_iterator::ptr postings(const flags& features) const = 0;
};

enum class SeekResult {
  FOUND = 0,
  NOT_FOUND,
  END
};

struct IRESEARCH_API seek_term_iterator : term_iterator {
  DECLARE_PTR(seek_term_iterator);
  DECLARE_FACTORY(seek_term_iterator);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief an empty struct tag type, parent for all seek_term_iterator cookies
  //////////////////////////////////////////////////////////////////////////////
  struct seek_cookie {
    DECLARE_PTR(seek_cookie);
    virtual ~seek_cookie() = default;
  }; // seek_cookie

  typedef seek_cookie::ptr cookie_ptr;

  virtual SeekResult seek_ge(const bytes_ref& value) = 0;
  virtual bool seek(const bytes_ref& value) = 0;

  virtual bool seek(
    const bytes_ref& term,
    const seek_cookie& cookie) = 0;
  virtual seek_cookie::ptr cookie() const = 0;
};

// ----------------------------------------------------------------------------
// --SECTION--                                                 iterator helpers
// ----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @brief jumps iterator to the specified target and returns current value
/// of the iterator
/// @returns 'false' if iterator exhausted, true otherwise
//////////////////////////////////////////////////////////////////////////////
template<typename Iterator, typename T, typename Less = std::less<T>>
bool seek(Iterator& it, const T& target, Less less = Less()) {
  bool next = true;
  while (less(it.value(), target) && true == (next = it.next()));
  return next;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief position iterator to the specified min term or to the next term
///        after the min term depending on the specified 'Include' value
/// @returns true in case if iterator has been succesfully positioned,
///          false otherwise
//////////////////////////////////////////////////////////////////////////////
template<bool Include>
inline bool seek_min(seek_term_iterator& it, const bytes_ref& min) {
  const auto res = it.seek_ge(min);

  return SeekResult::END != res
      && (Include || SeekResult::FOUND != res || it.next());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief position iterator 'count' items after the current position
/// @return if the iterator has been succesfully positioned
//////////////////////////////////////////////////////////////////////////////
template<typename Iterator>
inline bool skip(Iterator& itr, size_t count) {
  while (count--) {
    if (!itr.next()) {
      return false;
    }
  }

  return true;
}

NS_END

#endif