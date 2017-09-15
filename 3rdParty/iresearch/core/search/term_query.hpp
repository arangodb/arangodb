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

#ifndef IRESEARCH_TERM_QUERY_H
#define IRESEARCH_TERM_QUERY_H

#include "filter.hpp"
#include "cost.hpp"

#include "utils/string.hpp"

NS_ROOT

struct term_reader;

//////////////////////////////////////////////////////////////////////////////
/// @class term_state
/// @brief cached per reader term state
//////////////////////////////////////////////////////////////////////////////
struct reader_term_state {
  reader_term_state() = default;

  reader_term_state(reader_term_state&& rhs) NOEXCEPT
    : reader(rhs.reader),
      cookie(std::move(rhs.cookie)),
      estimation(rhs.estimation) {
    rhs.reader = nullptr;
    rhs.estimation = 0;
  }

  const term_reader* reader{};
  seek_term_iterator::cookie_ptr cookie;
  cost::cost_t estimation{};
}; // term_state

//////////////////////////////////////////////////////////////////////////////
/// @class range_query
/// @brief compiled query suitable for filters with a single term like "by_term"
//////////////////////////////////////////////////////////////////////////////
class term_query : public filter::prepared {
 public:
  typedef states_cache<reader_term_state> states_t;

  DECLARE_SPTR(term_query);

  static ptr make(
    const index_reader& rdr,
    const order::prepared& ord,
    filter::boost_t boost,
    const string_ref& field,
    const bytes_ref& term
  );

  explicit term_query(states_t&& states, attribute_store&& attrs);

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord) const override;

 private:
  states_cache<reader_term_state> states_;
}; // term_query

NS_END // ROOT

#endif
