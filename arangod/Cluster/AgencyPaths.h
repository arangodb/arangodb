////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_AGENCYPATHS_H
#define ARANGOD_CLUSTER_AGENCYPATHS_H

#include "PathComponent.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace arangodb {
namespace cluster {
namespace paths {

class Root;

class Arango : public PathComponent<Arango, Root> {
 public:
  constexpr char const* component() const noexcept { return "arango"; }

  using BaseType::PathComponent;

  class Plan : public PathComponent<Plan, Arango> {
   public:
    constexpr char const* component() const noexcept { return "Plan"; }

    using BaseType::PathComponent;

    class Databases : public PathComponent<Databases, Plan> {
     public:
      constexpr char const* component() const noexcept { return "Databases"; }

      using BaseType::PathComponent;

      class Database : public PathComponent<Database, Databases> {
       public:
        char const* component() const noexcept { return _name.c_str(); }

        explicit Database(ParentType const& parent, std::string name) noexcept
            : BaseType(parent), _name(std::move(name)) {}

       private:
        std::string const _name;
      };

      Database operator()(std::string name) noexcept {
        return Database(*this, std::move(name));
      }
    };

    Databases databases() const noexcept { return Databases(*this); }
  };

  Plan plan() const noexcept { return Plan(*this); }

  class Current : public PathComponent<Current, Arango> {
   public:
    constexpr char const* component() const noexcept { return "Current"; }

    using BaseType::PathComponent;

    class ServersRegistered : public PathComponent<ServersRegistered, Current> {
     public:
      constexpr char const* component() const noexcept {
        return "ServersRegistered";
      }

      using BaseType::PathComponent;
    };

    ServersRegistered serversRegistered() const noexcept {
      return ServersRegistered(*this);
    }
  };

  Current current() const noexcept { return Current(*this); }
};

// The root is no PathComponent, mainly because it has no parent and is the
// base case for recursions.
class Root {
 public:
  std::vector<std::string> pathVec(size_t size = 0) const {
    auto path = std::vector<std::string>();
    path.reserve(size);
    return path;
  }
  std::ostream& pathToStream(std::ostream& stream) const { return stream; }

  Root() : _arango(*this) {}

  constexpr Arango const& arango() const noexcept { return _arango; }

 private:
  Arango const _arango;
};

static Root const root{};
static Arango const& arango{root.arango()};

}  // namespace paths
}  // namespace cluster
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_AGENCYPATHS_H
