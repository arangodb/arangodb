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
#include "Activities/ActivityType.h"

#include <velocypack/Builder.h>
#include <Inspection/Status.h>

namespace arangodb::activities {

struct Activity : std::enable_shared_from_this<Activity> {
  Activity(ActivityId id, ActivityHandle parent, ActivityType type)
      : _id(std::move(id)),
        _parent(std::move(parent)),
        _type(std::move(type)) {}
  virtual ~Activity() = default;

  auto id() const noexcept -> ActivityId { return _id; };
  auto parent() const noexcept -> ActivityHandle { return _parent; }
  auto type() const noexcept -> ActivityType { return _type; }

  virtual auto snapshot(velocypack::Builder& builder) -> inspection::Status = 0;

 private:
  ActivityId _id;
  ActivityHandle _parent;
  ActivityType _type;
  //  std::string _database;
};

}  // namespace arangodb::activities
