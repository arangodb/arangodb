////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include <vector>

#include "shared.hpp"
#include "utils/noncopyable.hpp"

namespace irs {

//////////////////////////////////////////////////////////////////////////////
/// @class fst_states_map
/// @brief helper class for deduplication of fst states while building
///        minimal acyclic subsequential transducer
//////////////////////////////////////////////////////////////////////////////
template<typename Fst, typename State, typename PushState, typename Hash,
         typename StateEq, typename Fst::StateId NoStateId>
class fst_states_map : private util::noncopyable {
 public:
  using fst_type = Fst;
  using state_type = State;
  using state_id = typename fst_type::StateId;
  using push_state = PushState;
  using hasher = Hash;
  using state_equal = StateEq;

  explicit fst_states_map(size_t capacity = 16,
                          const push_state state_emplace = {},
                          const hasher& hash_function = {},
                          const state_equal& state_eq = {})
    : hasher_{hash_function},
      state_eq_{state_eq},
      push_state_{state_emplace},
      states_(capacity, NoStateId) {}

  state_id insert(const state_type& s, fst_type& fst) {
    const size_t mask = states_.size() - 1;
    size_t pos = hasher_(s, fst) % mask;
    for (;; ++pos, pos %= mask) {
      auto& bucket = states_[pos];

      if (NoStateId == bucket) {
        const state_id id = bucket = push_state_(s, fst);
        IRS_ASSERT(hasher_(s, fst) == hasher_(id, fst));
        ++count_;

        if (count_ > 2 * states_.size() / 3) {
          rehash(fst);
        }

        return id;
      }

      if (state_eq_(s, bucket, fst)) {
        return bucket;
      }
    }
  }

  void reset() noexcept {
    count_ = 0;
    std::fill(states_.begin(), states_.end(), NoStateId);
  }

 private:
  void rehash(const fst_type& fst) {
    std::vector<state_id> states(states_.size() * 2, NoStateId);
    const size_t mask = states.size() - 1;
    for (const auto id : states_) {
      if (NoStateId == id) {
        continue;
      }

      size_t pos = hasher_(id, fst) % mask;
      for (;; ++pos, pos %= mask) {
        auto& bucket = states[pos];

        if (NoStateId == bucket) {
          bucket = id;
          break;
        }
      }
    }

    states_ = std::move(states);
  }

  IRS_NO_UNIQUE_ADDRESS hasher hasher_;
  IRS_NO_UNIQUE_ADDRESS state_equal state_eq_;
  IRS_NO_UNIQUE_ADDRESS push_state push_state_;
  std::vector<state_id> states_;
  size_t count_{};
};

}  // namespace irs
