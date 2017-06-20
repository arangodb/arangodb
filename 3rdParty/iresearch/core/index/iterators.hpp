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
struct IRESEARCH_API doc_iterator:
  iterator<doc_id_t>, public util::const_attribute_store_provider {
  DECLARE_SPTR(doc_iterator);
  DECLARE_FACTORY(doc_iterator);

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

struct IRESEARCH_API score_doc_iterator: doc_iterator {
  DECLARE_PTR(score_doc_iterator);
  DECLARE_FACTORY(score_doc_iterator);

  static score_doc_iterator::ptr empty();

  virtual void score() = 0;
}; // score_doc_iterator

//////////////////////////////////////////////////////////////////////////////
/// @brief jumps iterator to the specified target and returns current value
/// of the iterator
//////////////////////////////////////////////////////////////////////////////
template<typename Iterator, typename T, typename Less = std::less<T>>
T seek(Iterator& it, const T& target, Less less = Less()) {
  while (less(it.value(), target) && it.next());
  return it.value();
}

// ----------------------------------------------------------------------------
// --SECTION--                                                  field iterators 
// ----------------------------------------------------------------------------

struct term_reader;

struct IRESEARCH_API field_iterator : iterator<const term_reader&> {
  DECLARE_MANAGED_PTR(field_iterator);

  static field_iterator::ptr empty();
};

struct column_meta;

struct IRESEARCH_API column_iterator : iterator<const column_meta&> {
  DECLARE_MANAGED_PTR(column_iterator);
  
  static column_iterator::ptr empty();
};

// ----------------------------------------------------------------------------
// --SECTION--                                                   term iterators 
// ----------------------------------------------------------------------------

struct IRESEARCH_API term_iterator
    : iterator<const bytes_ref&>,
      public util::const_attribute_store_provider {
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

NS_END

#endif
