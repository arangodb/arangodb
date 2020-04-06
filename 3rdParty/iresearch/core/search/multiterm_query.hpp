////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_MULTITERM_QUERY_H
#define IRESEARCH_MULTITERM_QUERY_H

#include "filter.hpp"
#include "cost.hpp"
#include "limited_sample_collector.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class multiterm_query
/// @brief compiled query suitable for filters with non adjacent set of terms
//////////////////////////////////////////////////////////////////////////////
class multiterm_query : public filter::prepared {
 public:
  typedef states_cache<multiterm_state> states_t;
  typedef std::vector<bstring> stats_t;

  DECLARE_SHARED_PTR(multiterm_query);

  explicit multiterm_query(states_t&& states,
                           std::shared_ptr<stats_t> const& stats,
                           boost_t boost,
                           sort::MergeType merge_type)


    : prepared(boost),
      states_(std::move(states)),
      stats_ptr_(stats),
      merge_type_(merge_type) {
    assert(stats_ptr_);
  }

  // multiterm_query will own stats
  explicit multiterm_query(states_t&& states, stats_t&& stats,
                           boost_t boost, sort::MergeType merge_type)
    : prepared(boost),
      states_(std::move(states)),
      stats_(std::move(stats)),
      stats_ptr_(std::shared_ptr<stats_t>(), &stats_),
      merge_type_(merge_type) {
    assert(stats_ptr_);
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_view& /*ctx*/) const override;

 private:
  const stats_t& stats() const noexcept {
    assert(stats_ptr_);
    return *stats_ptr_;
  }

  states_t states_;
  stats_t stats_;
  std::shared_ptr<stats_t> stats_ptr_;
  sort::MergeType merge_type_;
}; // multiterm_query

NS_END // ROOT

#endif // IRESEARCH_MULTITERM_QUERY_H
