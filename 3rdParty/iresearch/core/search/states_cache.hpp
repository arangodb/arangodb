////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_STATES_CACHE_H
#define IRESEARCH_STATES_CACHE_H

#include <vector>
#include <absl/container/flat_hash_map.h>

#include "shared.hpp"
#include "index/index_reader.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class states_cache
/// @brief generic cache for cached query states
/// @todo consider changing an API so that a sub_reader is indexable by an
///       integer. we can use a vector for lookup.
////////////////////////////////////////////////////////////////////////////////
template<typename State>
class states_cache : private util::noncopyable {
 public:
  using state_type = State;

  explicit states_cache(const index_reader& reader) {
    states_.reserve(reader.size());
  }

  states_cache(states_cache&&) = default;
  states_cache& operator=(states_cache&&) = default;

  state_type& insert(const sub_reader& rdr) {
    return states_[&rdr];
  }

  const state_type* find(const sub_reader& rdr) const noexcept {
    auto it = states_.find(&rdr);
    return states_.end() == it ? nullptr : &(it->second);
  }

  bool empty() const noexcept { return states_.empty(); }

 private:
  using states_map = absl::flat_hash_map<const sub_reader*, state_type>;

  // FIXME use vector instead?
  states_map states_;
}; // states_cache

}

#endif // IRESEARCH_STATES_CACHE_H
