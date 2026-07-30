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
#pragma once

#include "Activities/Activity.h"

namespace arangodb::activities {
/**
   Structure for the currently executing activity

   It adds the current thread to the activity-thread-list at construction and
   removes the thread on destruction.
 */
struct CurrentlyExecuting {
  CurrentlyExecuting(ActivityHandle handle);
  ~CurrentlyExecuting();
  CurrentlyExecuting(CurrentlyExecuting&& other) noexcept;
  auto operator=(CurrentlyExecuting&& other) noexcept -> CurrentlyExecuting&;
  CurrentlyExecuting(CurrentlyExecuting const& other) noexcept = delete;
  auto operator=(CurrentlyExecuting const& other) noexcept
      -> CurrentlyExecuting& = delete;
  ActivityHandle activity;

 private:
  Activity::ThreadListIterator _position;
};

}  // namespace arangodb::activities
