//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Activities/Activity.h"

#include <string>
#include <unordered_map>
#include <memory>

#include <velocypack/Builder.h>
#include "Inspection/Transformers.h"
#include "Inspection/VPack.h"
#include "Inspection/VPackSaveInspector.h"

#include "Basics/Guarded.h"

namespace arangodb::activities {

template<typename F, typename Data>
concept DataAccessor = requires(F f, Data& m) {
  {f(m)};
};
template<typename F, typename Data>
concept DataConstAccessor = requires(F f, Data const& m) {
  {f(m)};
};

template<typename Derived, typename Data>
struct GuardedActivity : Activity {
  GuardedActivity(ActivityId id, ActivityHandle parent, ActivityType type,
                  Data data)
      : Activity(std::move(id), std::move(parent), std::move(type)),
        _data(std::move(data)) {}

  auto data() const noexcept -> VPackBuilder override {
    auto builder = VPackBuilder{};
    auto data = _data.copy();
    velocypack::serialize(builder, data);
    return builder;
  }

  // Copying is not noexcept because it takes a lock, and that can throw
  auto copyData() const -> Data { return _data.copy(); }

  template<typename F>
  requires DataConstAccessor<F, Data>
  auto getData(F&& getter) const {
    return _data.doUnderLock(std::move(getter));
  }
  template<typename F>
  requires DataAccessor<F, Data>
  auto updateData(F&& mutator) { return _data.doUnderLock(std::move(mutator)); }

  using HandleType = std::shared_ptr<Derived>;

 protected:
  Guarded<Data> _data;
};

}  // namespace arangodb::activities
