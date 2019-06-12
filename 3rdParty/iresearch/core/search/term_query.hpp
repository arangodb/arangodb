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

  DECLARE_SHARED_PTR(term_query);

  static ptr make(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term
  );

  explicit term_query(states_t&& states, bstring&& stats, boost_t boost);

  virtual doc_iterator::ptr execute(
    const sub_reader& rdr,
    const order::prepared& ord,
    const attribute_view& /*ctx*/
  ) const override;

 private:
  states_cache<reader_term_state> states_;
}; // term_query

NS_END // ROOT

#endif
