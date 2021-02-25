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

#ifndef IRESEARCH_TERM_QUERY_H
#define IRESEARCH_TERM_QUERY_H

#include "search/filter.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class range_query
/// @brief compiled query suitable for filters with a single term like "by_term"
////////////////////////////////////////////////////////////////////////////////
class term_query final : public filter::prepared {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @class term_state
  /// @brief cached per reader term state
  //////////////////////////////////////////////////////////////////////////////
  struct term_state {
    const term_reader* reader{};
    seek_term_iterator::cookie_ptr cookie;
  }; // term_state

  typedef states_cache<term_state> states_t;

  explicit term_query(states_t&& states, bstring&& stats, boost_t boost);

  virtual doc_iterator::ptr execute(
    const sub_reader& rdr,
    const order::prepared& ord,
    const attribute_provider* /*ctx*/
  ) const override;

 private:
  states_cache<term_state> states_;
  bstring stats_;
}; // term_query

} // ROOT

#endif
