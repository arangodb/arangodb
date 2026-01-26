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

#include "ActivityRegistry/registry.h"
#include "ActivityRegistry/activity.h"

namespace arangodb::activity_registry {

Registry::ScopedDefaultParent::ScopedDefaultParent(Parent parent) noexcept {
  auto& local = Registry::defaultParent();
  _oldParent = std::move(local);
  local = std::move(parent);
}

Registry::ScopedDefaultParent::~ScopedDefaultParent() {
  auto& local = Registry::defaultParent();
  local = std::move(_oldParent);
}

}  // namespace arangodb::activity_registry
