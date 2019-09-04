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

#include "Basics/debugging.h"
#include "Cluster/ClusterTypes.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace arangodb {
namespace cluster {
namespace paths {

/**
 * Note that Root is the only class here that may be instantiated directly!
 *
 * Use e.g. as
 * auto root = Root();
 * std::string path = root.arango().plan().databases()("_system").pathStr();
 *
 * If you add anything, make sure to add tests in tests/Cluster/AgencyPathsTest.cpp.
 */

// The root is no PathComponent, mainly because it has no parent and is the
// base case for recursions.
class Root final : public Path {
 public:
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

         private:
          DatabaseID const _name;

          // Only the parent type P may instantiate a component, so make this private
          // and P a friend. MSVC ignores the friend declaration, though.
#if defined(_WIN32) || defined(_WIN64)
         public:
#else
         protected:
          friend ParentType;
#endif
          Database(std::shared_ptr<ParentType const> parent, DatabaseID name) noexcept
              : BaseType(std::move(parent)), _name(std::move(name)) {
            // An empty id would break the path creation, and we would get a
            // path to arango/Plan/Databases/. This could be all sorts of bad.
            // This would best be prevented by DatabaseID being a real type,
            // that disallows construction with an empty name.
            TRI_ASSERT(!_name.empty());
          }

          // shared ptr constructor
          static std::shared_ptr<Database const> make_shared(std::shared_ptr<ParentType const> parent,
                                                             DatabaseID name) {
            struct ConstructibleDatabase : public Database {
             public:
              ConstructibleDatabase(std::shared_ptr<ParentType const> parent,
                                    DatabaseID name) noexcept
                  : Database(std::move(parent), std::move(name)) {}
            };
            return std::make_shared<ConstructibleDatabase const>(std::move(parent),
                                                                 std::move(name));
          }
        };

        std::shared_ptr<Database const> database(DatabaseID name) const {
          return Database::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<Databases const> databases() const {
        return Databases::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Plan const> plan() const {
      return Plan::make_shared(shared_from_this());
    }

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

      std::shared_ptr<ServersRegistered const> serversRegistered() const {
        return ServersRegistered::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Current const> current() const {
      return Current::make_shared(shared_from_this());
    }
  };

  std::shared_ptr<Arango const> arango() const {
    // This builds a new root on purpose, as *this might not be on the heap.
    return Arango::make_shared(std::make_shared<Root>());
  }

 public:

  std::vector<std::string> _pathVec(size_t size) const final {
    auto path = std::vector<std::string>();
    path.reserve(size);
    return path;
  }

  std::ostream& pathToStream(std::ostream& stream) const final { return stream; }

  std::string pathStr() const final { return std::string{""}; }
};

}  // namespace paths
}  // namespace cluster
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_AGENCYPATHS_H
