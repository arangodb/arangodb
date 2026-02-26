////////////////////////////////////////////////////////////////////////////////
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

#include "Activities/Registry.h"
#include "Activities/Activity.h"
#include "Activities/ActivityHandle.h"
#include "velocypack/Builder.h"
#include "Basics/ErrorT.h"
#include "velocypack/SharedSlice.h"

namespace arangodb::activities {

auto Registry::setMetrics(std::shared_ptr<IRegistryMetrics> metrics) -> void {
  _metrics = std::move(metrics);
}

auto Registry::garbageCollect() -> void {
  _registry.doUnderLock([this](auto&& reg) {
    std::erase_if(reg, [](auto&& a) { return a.use_count() == 1; });
    _metrics->store_registered_nodes(reg.size());
  });
}

auto Registry::snapshot()
    -> errors::ErrorT<inspection::Status, velocypack::SharedSlice> {
  return _registry.doUnderLock([](auto&& reg) {
    VPackBuilder builder;
    {
      auto array = VPackArrayBuilder(&builder);
      for (auto&& a : reg) {
        auto res = a->snapshot(builder);
        if (!res.ok()) {
          return errors::ErrorT<inspection::Status,
                                velocypack::SharedSlice>::error(res.error());
        }
      }
    }
    return errors::ErrorT<inspection::Status, velocypack::SharedSlice>::ok(
        builder.sharedSlice());
  });
}

auto Registry::findActivityById(ActivityId id) const
    -> std::optional<ActivityHandle> {
  return _registry.doUnderLock(
      [id](auto&& reg) -> std::optional<ActivityHandle> {
        auto it = std::find_if(std::begin(reg), std::end(reg),
                               [id](auto a) { return a->id() == id; });
        if (it == std::end(reg)) {
          return std::nullopt;
        } else {
          return *it;
        }
      });
}

Registry::ScopedCurrentlyExecutingActivity::ScopedCurrentlyExecutingActivity(
    ActivityHandle activity) noexcept {
  _oldExecutingActivity = Registry::currentlyExecutingActivity();
  Registry::setCurrentlyExecutingActivity(activity);
}

Registry::ScopedCurrentlyExecutingActivity::
    ~ScopedCurrentlyExecutingActivity() {
  Registry::setCurrentlyExecutingActivity(_oldExecutingActivity);
}

}  // namespace arangodb::activities
