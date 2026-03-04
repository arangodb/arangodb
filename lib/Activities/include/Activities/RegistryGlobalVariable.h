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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Activities/ActivityHandle.h"
#include "Activities/Registry.h"

namespace arangodb::activities {

/**
   Global variable that holds all active activities.

   Includes a list of thread owned lists, one for each initialized
   thread.
 */
extern Registry registry;
extern const ActivityHandle Root;

template<typename T, typename... Args>
auto make(Args&&... args) -> T::HandleType {
  return registry.makeActivity<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
auto makeWithParent(ActivityHandle parent, Args&&... args) -> T::HandleType {
  return registry.makeActivityWithParent<T>(std::move(parent),
                                            std::forward<Args>(args)...);
}

}  // namespace arangodb::activities
