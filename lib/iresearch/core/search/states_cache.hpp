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

#pragma once

#include "resource_manager.hpp"
#include "shared.hpp"
#include "utils/noncopyable.hpp"

#include <absl/container/flat_hash_map.h>

namespace irs {

struct SubReader;

// Generic cache for cached query states
// TODO: consider changing an API so that a sub_reader is indexable by an
//       integer. we can use a vector for lookup.
template<typename State>
class StatesCache : private util::noncopyable {
 public:
  using state_type = State;

  explicit StatesCache(IResourceManager& memory, size_t size)
    : states_{Alloc{memory}} {
    states_.reserve(size);
  }

  StatesCache(StatesCache&&) = default;
  StatesCache& operator=(StatesCache&&) = default;

  state_type& insert(const SubReader& segment) {
    auto result =
      states_.emplace(&segment, states_.get_allocator().ResourceManager());
    return result.first->second;
  }

  const state_type* find(const SubReader& segment) const noexcept {
    const auto it = states_.find(&segment);
    return it != states_.end() ? &it->second : nullptr;
  }

  template<typename Pred>
  void erase_if(Pred pred) {
    absl::erase_if(states_, [&pred](const auto& v) { return pred(v.second); });
  }

  bool empty() const noexcept { return states_.empty(); }

 private:
  using Alloc =
    ManagedTypedAllocator<std::pair<const SubReader* const, state_type>>;

  using StatesMap = absl::flat_hash_map<
    const SubReader*, state_type,
    absl::container_internal::hash_default_hash<const SubReader*>,
    absl::container_internal::hash_default_eq<const SubReader*>, Alloc>;

  // FIXME use vector instead?
  StatesMap states_;
};

}  // namespace irs
