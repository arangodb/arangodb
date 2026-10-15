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

struct Snapshot {
  ActivityId id;
  std::optional<ActivityId> parentId;
  ActivityType type;
  ActivityCreated created;
  std::vector<basics::ThreadInfo> threads;
  VPackBuilder data;
};
template<typename Inspector>
auto inspect(Inspector& f, Snapshot& x) {
  return f.object(x).fields(
      f.field("id", x.id), f.field("parent", x.parentId),
      f.field("type", x.type),
      f.field("created", x.created)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field("threads", x.threads), f.field("data", x.data));
}

// We need a wrapper because the concurrent-registry needs a compile-time
// constant item type but our activities can have different types (all
// inheriting from Activity)
struct Activity;
struct ActivityPtr {
  Activity* a;

  using Snapshot = Snapshot;

  auto snapshot() const -> Snapshot;
};

struct Activity : containers::ThreadOwnedList<ActivityPtr>::Node {
  using Snapshot = Snapshot;
  using ThreadList = std::list<containers::SharedPtr<basics::ThreadInfo>>;
  using ThreadListIterator = ThreadList::iterator;
  Activity(ActivityId id, ActivityHandle parent, ActivityType type);
  virtual ~Activity() = default;

  auto id() const noexcept -> ActivityId { return _id; };
  auto parent() const noexcept -> ActivityHandle { return _parent; }
  auto parentId() const noexcept -> std::optional<ActivityId>;
  auto type() const noexcept -> ActivityType { return _type; }
  auto created() const noexcept -> ActivityCreated { return _created; }
  virtual auto data() const noexcept -> VPackBuilder {
    auto builder = VPackBuilder{};
    builder.openObject();
    builder.close();
    return builder;
  }
  auto threads() const noexcept -> std::vector<basics::ThreadInfo>;
  auto addCurrentThread() -> ThreadListIterator;
  auto removeThread(ThreadListIterator it) -> void;

  virtual auto snapshot(velocypack::Builder& builder) -> inspection::Status {
    return inspection::Status{};
  };
  auto snapshot() -> Snapshot {
    return Snapshot{.id = id(),
                    .parentId = parentId(),
                    .type = type(),
                    .created = created(),
                    .threads = threads(),
                    .data = data()};
  }

 private:
  ActivityId _id;
  ActivityHandle _parent;
  ActivityType _type;
  ActivityCreated _created;
  Guarded<ThreadList> _threads;
};

}  // namespace arangodb::activities
