////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iosfwd>

namespace arangodb {
namespace aql {
class ExecutionLocation {
 public:
  enum LocationType { ANYWHERE, DBSERVER, COORDINATOR };

 public:
  ExecutionLocation(LocationType location) : _location(location) {}
  bool canRunOnDBServer() const {
    return _location == LocationType::ANYWHERE || _location == LocationType::DBSERVER;
  }
  bool canRunOnCoordinator() const {
    return _location == LocationType::ANYWHERE || _location == LocationType::COORDINATOR;
  }

  friend auto operator<<(std::ostream& out, ExecutionLocation const& location) -> std::ostream&;

 private:
  LocationType _location;
};

}  // namespace aql
}  // namespace arangodb
