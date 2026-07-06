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

Registry::ScopedCurrentlyExecutingActivity::ScopedCurrentlyExecutingActivity(
    ActivityHandle activity) noexcept {
  _oldExecutingActivity = Registry::currentlyExecutingActivity();
  Registry::setCurrentlyExecutingActivity(activity);
}

Registry::ScopedCurrentlyExecutingActivity::
    ~ScopedCurrentlyExecutingActivity() {
  Registry::setCurrentlyExecutingActivity(_oldExecutingActivity);
}

auto Registry::snapshot()
    -> errors::ErrorT<inspection::Status, velocypack::SharedSlice> {
  inspection::Status errorStatus;
  VPackBuilder builder;
  builder.openArray();
  for_node([&](Activity::Snapshot activity) {
    auto inspector = inspection::VPackSaveInspector<>(builder);
    auto res = inspector.apply(activity);
    if (not res.ok()) {
      errorStatus = std::move(res);
    }
  });
  builder.close();
  if (not errorStatus.ok()) {
    return errors::ErrorT<inspection::Status, velocypack::SharedSlice>::error(
        errorStatus.error());
  }
  return errors::ErrorT<inspection::Status, velocypack::SharedSlice>::ok(
      builder.sharedSlice());
}

}  // namespace arangodb::activities
