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

#include "Activities/ActivityId.h"
#include "Activities/ActivityHandle.h"
#include "Activities/ActivityCreated.h"
#include "Activities/ActivityType.h"
#include "Basics/Guarded.h"
#include "Containers/Concurrent/shared.h"
#include "Containers/Concurrent/thread.h"
#include "Containers/Concurrent/ThreadOwnedList.h"
#include "Inspection/Transformers.h"

#include <velocypack/Builder.h>
#include <Inspection/Status.h>

#include <list>
#include <optional>

namespace arangodb::activities {

struct Activity : std::enable_shared_from_this<Activity> {
  using ThreadList = std::list<containers::SharedPtr<basics::ThreadInfo>>;
  using ThreadListIterator = ThreadList::iterator;
  Activity(ActivityId id, ActivityHandle parent, ActivityType type)
      : _id(std::move(id)),
        _parent(std::move(parent)),
        _type(std::move(type)),
        _created(std::chrono::system_clock::now()) {}
  virtual ~Activity() = default;

  auto id() const noexcept -> ActivityId { return _id; };
  auto parent() const noexcept -> ActivityHandle { return _parent; }
  auto parentId() const noexcept -> std::optional<ActivityId>;
  auto type() const noexcept -> ActivityType { return _type; }
  auto created() const noexcept -> ActivityCreated { return _created; }
  auto threads() const noexcept -> std::vector<basics::ThreadInfo>;
  auto addCurrentThread() -> ThreadListIterator;
  auto removeThread(ThreadListIterator it) -> void;

  virtual auto snapshot(velocypack::Builder& builder) -> inspection::Status = 0;

 private:
  ActivityId _id;
  ActivityHandle _parent;
  ActivityType _type;
  ActivityCreated _created;
  Guarded<ThreadList> _threads;
};

}  // namespace arangodb::activities
