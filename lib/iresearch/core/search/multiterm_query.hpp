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

#pragma once

#include "search/filter.hpp"
#include "search/states/multiterm_state.hpp"
#include "search/states_cache.hpp"

namespace irs {

// Compiled query suitable for filters with non adjacent set of terms.
class MultiTermQuery : public filter::prepared {
 public:
  using States = StatesCache<MultiTermState>;
  // TODO(MBkkt) block_pool<byte>
  using Stats = ManagedVector<bstring>;

  explicit MultiTermQuery(States&& states, Stats&& stats, score_t boost,
                          ScoreMergeType merge_type, size_t min_match)
    : states_{std::move(states)},
      stats_{std::move(stats)},
      boost_{boost},
      merge_type_{merge_type},
      min_match_{min_match} {}

  doc_iterator::ptr execute(const ExecutionContext& ctx) const final;

  void visit(const SubReader& segment, PreparedStateVisitor& visitor,
             score_t boost) const final;

  score_t boost() const noexcept final { return boost_; }

 private:
  States states_;
  Stats stats_;
  score_t boost_;
  ScoreMergeType merge_type_;
  size_t min_match_;
};

}  // namespace irs
