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
 * Note that no class here may be instantiated directly! You can only call root() and work your way down from there.
 *
 * Use e.g. as
 *   std::string path = root()->arango()->plan()->databases()->database("_system")->str();
 *   // path == "/arango/Plan/Databases/_system"
 * or
 *   std::vector<std::string> path = root()->arango()->plan()->databases()->database("_system")->vec();
 *   // path == {"arango", "Plan", "Databases", "_system"}
 * or
 *   std::stringstream stream;
 *   stream << *root()->arango()->plan()->databases()->database("_system");
 *   // stream.str() == "/arango/Plan/Databases/_system"
 * or
 *   std::stringstream stream;
 *   root()->arango()->plan()->databases()->database("_system")->toStream(stream);
 *   // stream.str() == "/arango/Plan/Databases/_system"
 *
 * If you add anything, make sure to add tests in tests/Cluster/AgencyPathsTest.cpp.
 *
 * An example for a static component looks like this:
 * class SomeOuterClass {
 * ...
 *   class YourComponent : public PathComponent<YourComponent, SomeOuterClass> {
 *    public:
 *     constexpr char const* component() const noexcept { return "YourComponent"; }
 *
 *     using BaseType::PathComponent;
 *
 *     // Add possible inner classes here
 *   };
 *
 *   std::shared_ptr<YourComponent const> yourComponent() const {
 *     return YourComponent::make_shared(shared_from_this());
 *   }
 * ...
 * }
 *
 */

class Root;

inline std::shared_ptr<Root const> root();

// The root is no PathComponent, mainly because it has no parent and is the
// base case for recursions.
class Root : public std::enable_shared_from_this<Root>, public Path {
 public:
  void forEach(std::function<void(char const* component)> const&) const final {}

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

    class Supervision : public PathComponent<Supervision, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Supervision"; }

      using BaseType::PathComponent;

      class State : public PathComponent<State, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "State"; }

        using BaseType::PathComponent;

        class Timestamp : public PathComponent<Timestamp, State> {
         public:
          constexpr char const* component() const noexcept {
            return "Timestamp";
          }

          using BaseType::PathComponent;
        };

        std::shared_ptr<Timestamp const> timestamp() const {
          return Timestamp::make_shared(shared_from_this());
        }

        class Mode : public PathComponent<Mode, State> {
         public:
          constexpr char const* component() const noexcept { return "Mode"; }

          using BaseType::PathComponent;
        };

        std::shared_ptr<Mode const> mode() const {
          return Mode::make_shared(shared_from_this());
        }
      };

      std::shared_ptr<State const> state() const {
        return State::make_shared(shared_from_this());
      }

      class Shards : public PathComponent<Shards, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "Shards"; }

        using BaseType::PathComponent;
      };

      std::shared_ptr<Shards const> shards() const {
        return Shards::make_shared(shared_from_this());
      }

      class DbServers : public PathComponent<DbServers, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "DBServers"; }

        using BaseType::PathComponent;
      };

      std::shared_ptr<DbServers const> dbServers() const {
        return DbServers::make_shared(shared_from_this());
      }

      class Health : public PathComponent<Health, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "Health"; }

        using BaseType::PathComponent;

        class DbServer : public PathComponent<DbServer, Health> {
         public:
          char const* component() const noexcept { return _name.c_str(); }

         private:
          ServerID const _name;

          // Only the parent type P may instantiate a component, so make this private
          // and P a friend. MSVC ignores the friend declaration, though.
#if defined(_WIN32) || defined(_WIN64)
         public:
#else
         protected:
          friend ParentType;
#endif
          DbServer(std::shared_ptr<ParentType const> parent, ServerID name) noexcept
              : BaseType(std::move(parent)), _name(std::move(name)) {
            // An empty id would break the path creation, and we would get a
            // path to arango/Plan/Databases/. This could be all sorts of bad.
            // This would best be prevented by DatabaseID being a real type,
            // that disallows construction with an empty name.
            TRI_ASSERT(!_name.empty());
          }

          // shared ptr constructor
          static std::shared_ptr<DbServer const> make_shared(std::shared_ptr<ParentType const> parent,
                                                             ServerID name) {
            struct ConstructibleDbServer : public DbServer {
             public:
              ConstructibleDbServer(std::shared_ptr<ParentType const> parent, ServerID name) noexcept
                  : DbServer(std::move(parent), std::move(name)) {}
            };
            return std::make_shared<ConstructibleDbServer const>(std::move(parent),
                                                                 std::move(name));
          }

         public:
          class SyncTime : public PathComponent<SyncTime, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "SyncTime";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<SyncTime const> syncTime() const {
            return SyncTime::make_shared(shared_from_this());
          }

          class Timestamp : public PathComponent<Timestamp, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Timestamp";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<Timestamp const> timestamp() const {
            return Timestamp::make_shared(shared_from_this());
          }

          class SyncStatus : public PathComponent<SyncStatus, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "SyncStatus";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<SyncStatus const> syncStatus() const {
            return SyncStatus::make_shared(shared_from_this());
          }

          class LastAckedTime : public PathComponent<LastAckedTime, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "LastAckedTime";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<LastAckedTime const> lastAckedTime() const {
            return LastAckedTime::make_shared(shared_from_this());
          }

          class Host : public PathComponent<Host, DbServer> {
           public:
            constexpr char const* component() const noexcept { return "Host"; }

            using BaseType::PathComponent;
          };

          std::shared_ptr<Host const> host() const {
            return Host::make_shared(shared_from_this());
          }

          class Engine : public PathComponent<Engine, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Engine";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<Engine const> engine() const {
            return Engine::make_shared(shared_from_this());
          }

          class Version : public PathComponent<Version, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Version";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<Version const> version() const {
            return Version::make_shared(shared_from_this());
          }

          class Status : public PathComponent<Status, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Status";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<Status const> status() const {
            return Status::make_shared(shared_from_this());
          }

          class ShortName : public PathComponent<ShortName, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "ShortName";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<ShortName const> shortName() const {
            return ShortName::make_shared(shared_from_this());
          }

          class Endpoint : public PathComponent<Endpoint, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Endpoint";
            }

            using BaseType::PathComponent;
          };

          std::shared_ptr<Endpoint const> endpoint() const {
            return Endpoint::make_shared(shared_from_this());
          }
        };

        std::shared_ptr<DbServer const> dbServer(ServerID server) const {
          return DbServer::make_shared(shared_from_this(), std::move(server));
        }
      };

      std::shared_ptr<Health const> health() const {
        return Health::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Supervision const> supervision() const {
      return Supervision::make_shared(shared_from_this());
    }
  };

  std::shared_ptr<Arango const> arango() const {
    return Arango::make_shared(shared_from_this());
  }

 private:
  // May only be constructed by root()
  friend std::shared_ptr<Root const> root();
  Root() = default;
  static std::shared_ptr<Root const> make_shared() {
    struct ConstructibleRoot : public Root {
     public:
      explicit ConstructibleRoot() noexcept = default;
    };
    return std::make_shared<ConstructibleRoot const>();
  }
};

std::shared_ptr<Root const> root() { return Root::make_shared(); }

}  // namespace paths
}  // namespace cluster
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_AGENCYPATHS_H
