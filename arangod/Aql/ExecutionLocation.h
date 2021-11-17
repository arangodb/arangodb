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
  enum LocationType { ANYWHERE, DBSERVER, COORDINATOR, SUBQUERY_START, SUBQUERY_END, REQUIRES_CONTEXT };

 public:
  ExecutionLocation(LocationType location) : _location(location) {}
  bool canRunOnDBServer() const {
    return _location != LocationType::COORDINATOR;
  }
  bool canRunOnCoordinator() const {
    return _location != LocationType::DBSERVER;
  }

  bool isSubqueryStart() const {
    return _location == LocationType::SUBQUERY_START;
  }

  bool isSubqueryEnd() const {
    return _location == LocationType::SUBQUERY_END;
  }

  /**
   * @brief A strict ExecutionLocation needs to be
   * concidered to determine positioning of nodes
   * If it is not strict we can easily ignore the
   * Node in the location planning.
   */
  bool isStrict() const {
    return _location != LocationType::ANYWHERE && _location != LocationType::REQUIRES_CONTEXT;
  }

  /**
   * @brief a requiresContext ExecutionLocation can
   * in theory be executed everywhere, but has sideeffects
   * on it's surrounding. It may also be restricted to a location
   * if the surrounding does not match.
   */
  bool requiresContext() const {
    return _location == LocationType::REQUIRES_CONTEXT;
  }

  friend auto operator<<(std::ostream& out, ExecutionLocation const& location) -> std::ostream&;

 private:
  LocationType _location;
};

}  // namespace aql
}  // namespace arangodb
