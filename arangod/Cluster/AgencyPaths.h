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
 *   // Add your component
 *   class YourComponent : public StaticComponent<YourComponent, SomeOuterClass> {
 *    public:
 *     constexpr char const* component() const noexcept { return "YourComponent"; }
 *
 *     // Inherit constructors
 *     using BaseType::StaticComponent;
 *
 *     // Add possible inner classes here
 *   };
 *
 *   // Add an accessor to it in the outer class
 *   std::shared_ptr<YourComponent const> yourComponent() const {
 *     return YourComponent::make_shared(shared_from_this());
 *   }
 * ...
 * }
 *
 * An example for a dynamic component looks like this, here holding a value of type SomeType:
 * class SomeOuterClass {
 * ...
 *   // Add your component
 *   class YourComponent : public DynamicComponent<YourComponent, SomeOuterClass, SomeType> {
 *    public:
 *     // Access your SomeType value with value():
 *     char const* component() const noexcept { return value().c_str(); }
 *
 *     // Inherit constructors
 *     using BaseType::DynamicComponent;
 *   };
 *
 *   // Add an accessor to it in the outer class
 *   std::shared_ptr<YourComponent const> yourComponent(DatabaseID name) const {
 *     return YourComponent::make_shared(shared_from_this(), std::move(name));
 *   }
 * ...
 * }
 *
 */

/*
class YourComponent : public StaticComponent<YourComponent, SomeOuterClass> {
 public:
  constexpr char const* component() const noexcept { return "YourComponent"; }

  using BaseType::StaticComponent;
};

std::shared_ptr<YourComponent const> yourComponent() const {
  return YourComponent::make_shared(shared_from_this());
}
*/

class Root;

inline std::shared_ptr<Root const> root();

// The root is no StaticComponent, mainly because it has no parent and is the
// base case for recursions.
class Root : public std::enable_shared_from_this<Root>, public Path {
 public:
  void forEach(std::function<void(char const* component)> const&) const final {}

 public:
  class Arango : public StaticComponent<Arango, Root> {
   public:
    constexpr char const* component() const noexcept { return "arango"; }

    using BaseType::StaticComponent;

    class Plan : public StaticComponent<Plan, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Plan"; }

      using BaseType::StaticComponent;

      class Databases : public StaticComponent<Databases, Plan> {
       public:
        constexpr char const* component() const noexcept { return "Databases"; }

        using BaseType::StaticComponent;

        class Database : public DynamicComponent<Database, Databases, DatabaseID> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;
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

    class Current : public StaticComponent<Current, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Current"; }

      using BaseType::StaticComponent;

      class ServersRegistered : public StaticComponent<ServersRegistered, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "ServersRegistered";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<ServersRegistered const> serversRegistered() const {
        return ServersRegistered::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Current const> current() const {
      return Current::make_shared(shared_from_this());
    }

    class Supervision : public StaticComponent<Supervision, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Supervision"; }

      using BaseType::StaticComponent;

      class State : public StaticComponent<State, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "State"; }

        using BaseType::StaticComponent;

        class Timestamp : public StaticComponent<Timestamp, State> {
         public:
          constexpr char const* component() const noexcept {
            return "Timestamp";
          }

          using BaseType::StaticComponent;
        };

        std::shared_ptr<Timestamp const> timestamp() const {
          return Timestamp::make_shared(shared_from_this());
        }

        class Mode : public StaticComponent<Mode, State> {
         public:
          constexpr char const* component() const noexcept { return "Mode"; }

          using BaseType::StaticComponent;
        };

        std::shared_ptr<Mode const> mode() const {
          return Mode::make_shared(shared_from_this());
        }
      };

      std::shared_ptr<State const> state() const {
        return State::make_shared(shared_from_this());
      }

      class Shards : public StaticComponent<Shards, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "Shards"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Shards const> shards() const {
        return Shards::make_shared(shared_from_this());
      }

      class DbServers : public StaticComponent<DbServers, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "DBServers"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<DbServers const> dbServers() const {
        return DbServers::make_shared(shared_from_this());
      }

      class Health : public StaticComponent<Health, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "Health"; }

        using BaseType::StaticComponent;

        class DbServer : public DynamicComponent<DbServer, Health, ServerID> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;

          class SyncTime : public StaticComponent<SyncTime, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "SyncTime";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<SyncTime const> syncTime() const {
            return SyncTime::make_shared(shared_from_this());
          }

          class Timestamp : public StaticComponent<Timestamp, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Timestamp";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Timestamp const> timestamp() const {
            return Timestamp::make_shared(shared_from_this());
          }

          class SyncStatus : public StaticComponent<SyncStatus, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "SyncStatus";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<SyncStatus const> syncStatus() const {
            return SyncStatus::make_shared(shared_from_this());
          }

          class LastAckedTime : public StaticComponent<LastAckedTime, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "LastAckedTime";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<LastAckedTime const> lastAckedTime() const {
            return LastAckedTime::make_shared(shared_from_this());
          }

          class Host : public StaticComponent<Host, DbServer> {
           public:
            constexpr char const* component() const noexcept { return "Host"; }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Host const> host() const {
            return Host::make_shared(shared_from_this());
          }

          class Engine : public StaticComponent<Engine, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Engine";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Engine const> engine() const {
            return Engine::make_shared(shared_from_this());
          }

          class Version : public StaticComponent<Version, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Version";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Version const> version() const {
            return Version::make_shared(shared_from_this());
          }

          class Status : public StaticComponent<Status, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Status";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Status const> status() const {
            return Status::make_shared(shared_from_this());
          }

          class ShortName : public StaticComponent<ShortName, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "ShortName";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<ShortName const> shortName() const {
            return ShortName::make_shared(shared_from_this());
          }

          class Endpoint : public StaticComponent<Endpoint, DbServer> {
           public:
            constexpr char const* component() const noexcept {
              return "Endpoint";
            }

            using BaseType::StaticComponent;
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
