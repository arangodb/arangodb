////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Agency/PathComponent.h"
#include "Basics/debugging.h"
#include "Cluster/ClusterTypes.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Build agency paths in a compile-time safe manner.
 *
 * Usage:
 *
 *   using namespace arangodb::cluster::paths;
 *
 *   std::string path = root()->arango()->plan()->databases()->database("_system")->str();
 *   // path == "/arango/Plan/Databases/_system"
 *   ...
 *   std::vector<std::string> path = root()->arango()->plan()->databases()->database("_system")->vec();
 *   // path == {"arango", "Plan", "Databases", "_system"}
 *   ...
 *   std::stringstream stream;
 *   stream << *root()->arango()->plan()->databases()->database("_system");
 *   // stream.str() == "/arango/Plan/Databases/_system"
 *   ...
 *   std::stringstream stream;
 *   root()->arango()->plan()->databases()->database("_system")->toStream(stream);
 *   // stream.str() == "/arango/Plan/Databases/_system"
 *
 * Or use shorthands:
 *
 *   using namespace arangodb::cluster::paths::aliases;
 *
 *   arango()->initCollectionsDone();
 *   plan()->databases();
 *   current()->serversKnown();
 *   target()->pending();
 *   supervision()->health();
 *
 * @details
 * Note that no class here may be instantiated directly! You can only call root() and work your way down from there.
 *
 * If you add anything, make sure to add tests in tests/Cluster/AgencyPathsTest.cpp.
 *
 * An example for a static component looks like this:
 *   class SomeOuterClass {
 *   ...
 *     // Add your component
 *     class YourComponent : public StaticComponent<YourComponent, SomeOuterClass> {
 *      public:
 *       constexpr char const* component() const noexcept { return "YourComponent"; }
 *
 *       // Inherit constructors
 *       using BaseType::StaticComponent;
 *
 *       // Add possible inner classes here
 *     };
 *
 *     // Add an accessor to it in the outer class
 *     std::shared_ptr<YourComponent const> yourComponent() const {
 *       return YourComponent::make_shared(shared_from_this());
 *     }
 *   ...
 *   }
 *
 * An example for a dynamic component looks like this, here holding a value of type SomeType:
 *   class SomeOuterClass {
 *   ...
 *     // Add your component
 *     class YourComponent : public DynamicComponent<YourComponent, SomeOuterClass, SomeType> {
 *      public:
 *       // Access your SomeType value with value():
 *       char const* component() const noexcept { return value().c_str(); }
 *
 *       // Inherit constructors
 *       using BaseType::DynamicComponent;
 *     };
 *
 *     // Add an accessor to it in the outer class
 *     std::shared_ptr<YourComponent const> yourComponent(DatabaseID name) const {
 *       return YourComponent::make_shared(shared_from_this(), std::move(name));
 *     }
 *   ...
 *   }
 *
 */

namespace arangodb::cluster::paths {

class Root;

auto root() -> std::shared_ptr<Root const>;

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

      class Version : public StaticComponent<Version, Plan> {
       public:
        constexpr char const* component() const noexcept { return "Version"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Version const> version() const {
        return Version::make_shared(shared_from_this());
      }

      class Views : public StaticComponent<Views, Plan> {
       public:
        constexpr char const* component() const noexcept { return "Views"; }

        using BaseType::StaticComponent;

        class Database : public DynamicComponent<Database, Views, DatabaseID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Database const> database(DatabaseID name) const {
          return Database::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<Views const> views() const {
        return Views::make_shared(shared_from_this());
      }

      class AsyncReplication : public StaticComponent<AsyncReplication, Plan> {
       public:
        constexpr char const* component() const noexcept {
          return "AsyncReplication";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<AsyncReplication const> asyncReplication() const {
        return AsyncReplication::make_shared(shared_from_this());
      }

      class Coordinators : public StaticComponent<Coordinators, Plan> {
       public:
        constexpr char const* component() const noexcept {
          return "Coordinators";
        }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, Coordinators, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<Coordinators const> coordinators() const {
        return Coordinators::make_shared(shared_from_this());
      }

      class Lock : public StaticComponent<Lock, Plan> {
       public:
        constexpr char const* component() const noexcept { return "Lock"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Lock const> lock() const {
        return Lock::make_shared(shared_from_this());
      }

      class Singles : public StaticComponent<Singles, Plan> {
       public:
        constexpr char const* component() const noexcept { return "Singles"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Singles const> singles() const {
        return Singles::make_shared(shared_from_this());
      }

      class DbServers : public StaticComponent<DbServers, Plan> {
       public:
        constexpr char const* component() const noexcept { return "DBServers"; }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, DbServers, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<DbServers const> dBServers() const {
        return DbServers::make_shared(shared_from_this());
      }

      class Collections : public StaticComponent<Collections, Plan> {
       public:
        constexpr char const* component() const noexcept {
          return "Collections";
        }

        using BaseType::StaticComponent;

        class Database : public DynamicComponent<Database, Collections, DatabaseID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;

          class Collection : public DynamicComponent<Collection, Database, CollectionID> {
           public:
            char const* component() const noexcept {
              return value().c_str();
            }

            using BaseType::DynamicComponent;

            class WaitForSync : public StaticComponent<WaitForSync, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "waitForSync";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<WaitForSync const> waitForSync() const {
              return WaitForSync::make_shared(shared_from_this());
            }

            class Type : public StaticComponent<Type, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "type";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Type const> type() const {
              return Type::make_shared(shared_from_this());
            }

            class Status : public StaticComponent<Status, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "status";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Status const> status() const {
              return Status::make_shared(shared_from_this());
            }

            class Shards : public StaticComponent<Shards, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "shards";
              }

              using BaseType::StaticComponent;

              class Shard : public DynamicComponent<Shard, Shards, ShardID> {
               public:
                char const* component() const noexcept {
                  return value().c_str();
                }

                using BaseType::DynamicComponent;
              };

              std::shared_ptr<Shard const> shard(ShardID name) const {
                return Shard::make_shared(shared_from_this(), std::move(name));
              }
            };

            std::shared_ptr<Shards const> shards() const {
              return Shards::make_shared(shared_from_this());
            }

            class StatusString : public StaticComponent<StatusString, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "statusString";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<StatusString const> statusString() const {
              return StatusString::make_shared(shared_from_this());
            }

            class ShardingStrategy : public StaticComponent<ShardingStrategy, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "shardingStrategy";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<ShardingStrategy const> shardingStrategy() const {
              return ShardingStrategy::make_shared(shared_from_this());
            }

            class ShardKeys : public StaticComponent<ShardKeys, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "shardKeys";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<ShardKeys const> shardKeys() const {
              return ShardKeys::make_shared(shared_from_this());
            }

            class ReplicationFactor : public StaticComponent<ReplicationFactor, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "replicationFactor";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<ReplicationFactor const> replicationFactor() const {
              return ReplicationFactor::make_shared(shared_from_this());
            }

            class NumberOfShards : public StaticComponent<NumberOfShards, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "numberOfShards";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<NumberOfShards const> numberOfShards() const {
              return NumberOfShards::make_shared(shared_from_this());
            }

            class KeyOptions : public StaticComponent<KeyOptions, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "keyOptions";
              }

              using BaseType::StaticComponent;

              class Type : public StaticComponent<Type, KeyOptions> {
               public:
                constexpr char const* component() const noexcept {
                  return "type";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<Type const> type() const {
                return Type::make_shared(shared_from_this());
              }

              class AllowUserKeys : public StaticComponent<AllowUserKeys, KeyOptions> {
               public:
                constexpr char const* component() const noexcept {
                  return "allowUserKeys";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<AllowUserKeys const> allowUserKeys() const {
                return AllowUserKeys::make_shared(shared_from_this());
              }
            };

            std::shared_ptr<KeyOptions const> keyOptions() const {
              return KeyOptions::make_shared(shared_from_this());
            }

            class IsSystem : public StaticComponent<IsSystem, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "isSystem";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<IsSystem const> isSystem() const {
              return IsSystem::make_shared(shared_from_this());
            }

            class Name : public StaticComponent<Name, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "name";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Name const> name() const {
              return Name::make_shared(shared_from_this());
            }

            class Indexes : public StaticComponent<Indexes, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "indexes";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Indexes const> indexes() const {
              return Indexes::make_shared(shared_from_this());
            }

            class IsSmart : public StaticComponent<IsSmart, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "isSmart";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<IsSmart const> isSmart() const {
              return IsSmart::make_shared(shared_from_this());
            }

            class Id : public StaticComponent<Id, Collection> {
             public:
              constexpr char const* component() const noexcept { return "id"; }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Id const> id() const {
              return Id::make_shared(shared_from_this());
            }

            class DistributeShardsLike
                : public StaticComponent<DistributeShardsLike, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "distributeShardsLike";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<DistributeShardsLike const> distributeShardsLike() const {
              return DistributeShardsLike::make_shared(shared_from_this());
            }

            class Deleted : public StaticComponent<Deleted, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "deleted";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Deleted const> deleted() const {
              return Deleted::make_shared(shared_from_this());
            }

            class WriteConcern
                : public StaticComponent<WriteConcern, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "writeConcern";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<WriteConcern const> writeConcern() const {
              return WriteConcern::make_shared(shared_from_this());
            }

            class CacheEnabled : public StaticComponent<CacheEnabled, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "cacheEnabled";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<CacheEnabled const> cacheEnabled() const {
              return CacheEnabled::make_shared(shared_from_this());
            }

            class IsBuilding : public StaticComponent<IsBuilding, Collection> {
             public:
              constexpr char const* component() const noexcept {
                return "isBuilding";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<IsBuilding const> isBuilding() const {
              return IsBuilding::make_shared(shared_from_this());
            }
          };

          std::shared_ptr<Collection const> collection(CollectionID name) const {
            return Collection::make_shared(shared_from_this(), std::move(name));
          }
        };

        std::shared_ptr<Database const> database(DatabaseID name) const {
          return Database::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<Collections const> collections() const {
        return Collections::make_shared(shared_from_this());
      }

      class Databases : public StaticComponent<Databases, Plan> {
       public:
        constexpr char const* component() const noexcept { return "Databases"; }

        using BaseType::StaticComponent;

        class Database : public DynamicComponent<Database, Databases, DatabaseID> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;

          class Name : public StaticComponent<Name, Database> {
           public:
            constexpr char const* component() const noexcept { return "name"; }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Name const> name() const {
            return Name::make_shared(shared_from_this());
          }

          class Id : public StaticComponent<Id, Database> {
           public:
            constexpr char const* component() const noexcept { return "id"; }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Id const> id() const {
            return Id::make_shared(shared_from_this());
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

    class Current : public StaticComponent<Current, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Current"; }

      using BaseType::StaticComponent;

      class Version : public StaticComponent<Version, Current> {
       public:
        constexpr char const* component() const noexcept { return "Version"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Version const> version() const {
        return Version::make_shared(shared_from_this());
      }

      class ServersKnown : public StaticComponent<ServersKnown, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "ServersKnown";
        }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, ServersKnown, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;

          class RebootId : public StaticComponent<RebootId, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "rebootId";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<RebootId const> rebootId() const {
            return RebootId::make_shared(shared_from_this());
          }
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<ServersKnown const> serversKnown() const {
        return ServersKnown::make_shared(shared_from_this());
      }

      class FoxxmasterQueueupdate : public StaticComponent<FoxxmasterQueueupdate, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "FoxxmasterQueueupdate";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<FoxxmasterQueueupdate const> foxxmasterQueueupdate() const {
        return FoxxmasterQueueupdate::make_shared(shared_from_this());
      }

      class ShardsCopied : public StaticComponent<ShardsCopied, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "ShardsCopied";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<ShardsCopied const> shardsCopied() const {
        return ShardsCopied::make_shared(shared_from_this());
      }

      class Foxxmaster : public StaticComponent<Foxxmaster, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "Foxxmaster";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Foxxmaster const> foxxmaster() const {
        return Foxxmaster::make_shared(shared_from_this());
      }

      class ServersRegistered : public StaticComponent<ServersRegistered, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "ServersRegistered";
        }

        using BaseType::StaticComponent;

        class Version : public StaticComponent<Version, ServersRegistered> {
         public:
          constexpr char const* component() const noexcept { return "Version"; }

          using BaseType::StaticComponent;
        };

        std::shared_ptr<Version const> version() const {
          return Version::make_shared(shared_from_this());
        }

        class Server : public DynamicComponent<Server, ServersRegistered, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;

          class Timestamp : public StaticComponent<Timestamp, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "timestamp";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Timestamp const> timestamp() const {
            return Timestamp::make_shared(shared_from_this());
          }

          class Engine : public StaticComponent<Engine, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "engine";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Engine const> engine() const {
            return Engine::make_shared(shared_from_this());
          }

          class Endpoint : public StaticComponent<Endpoint, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "endpoint";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Endpoint const> endpoint() const {
            return Endpoint::make_shared(shared_from_this());
          }

          class Host : public StaticComponent<Host, Server> {
           public:
            constexpr char const* component() const noexcept { return "host"; }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Host const> host() const {
            return Host::make_shared(shared_from_this());
          }

          class VersionString : public StaticComponent<VersionString, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "versionString";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<VersionString const> versionString() const {
            return VersionString::make_shared(shared_from_this());
          }

          class AdvertisedEndpoint : public StaticComponent<AdvertisedEndpoint, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "advertisedEndpoint";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<AdvertisedEndpoint const> advertisedEndpoint() const {
            return AdvertisedEndpoint::make_shared(shared_from_this());
          }

          class Version : public StaticComponent<Version, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "version";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Version const> version() const {
            return Version::make_shared(shared_from_this());
          }
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<ServersRegistered const> serversRegistered() const {
        return ServersRegistered::make_shared(shared_from_this());
      }

      class NewServers : public StaticComponent<NewServers, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "NewServers";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<NewServers const> newServers() const {
        return NewServers::make_shared(shared_from_this());
      }

      class AsyncReplication : public StaticComponent<AsyncReplication, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "AsyncReplication";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<AsyncReplication const> asyncReplication() const {
        return AsyncReplication::make_shared(shared_from_this());
      }

      class Coordinators : public StaticComponent<Coordinators, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "Coordinators";
        }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, Coordinators, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<Coordinators const> coordinators() const {
        return Coordinators::make_shared(shared_from_this());
      }

      class Lock : public StaticComponent<Lock, Current> {
       public:
        constexpr char const* component() const noexcept { return "Lock"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Lock const> lock() const {
        return Lock::make_shared(shared_from_this());
      }

      class Singles : public StaticComponent<Singles, Current> {
       public:
        constexpr char const* component() const noexcept { return "Singles"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Singles const> singles() const {
        return Singles::make_shared(shared_from_this());
      }

      class DbServers : public StaticComponent<DbServers, Current> {
       public:
        constexpr char const* component() const noexcept { return "DBServers"; }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, DbServers, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<DbServers const> dBServers() const {
        return DbServers::make_shared(shared_from_this());
      }

      class Collections : public StaticComponent<Collections, Current> {
       public:
        constexpr char const* component() const noexcept {
          return "Collections";
        }

        using BaseType::StaticComponent;

        class Database : public DynamicComponent<Database, Collections, DatabaseID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;

          class Collection : public DynamicComponent<Collection, Database, CollectionID> {
           public:
            char const* component() const noexcept {
              return value().c_str();
            }

            using BaseType::DynamicComponent;

            class Shard : public DynamicComponent<Shard, Collection, ShardID> {
             public:
              char const* component() const noexcept {
                return value().c_str();
              }

              using BaseType::DynamicComponent;

              class Servers : public StaticComponent<Servers, Shard> {
               public:
                constexpr char const* component() const noexcept {
                  return "servers";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<Servers const> servers() const {
                return Servers::make_shared(shared_from_this());
              }

              class Indexes : public StaticComponent<Indexes, Shard> {
               public:
                constexpr char const* component() const noexcept {
                  return "indexes";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<Indexes const> indexes() const {
                return Indexes::make_shared(shared_from_this());
              }

              class FailoverCandidates : public StaticComponent<FailoverCandidates, Shard> {
               public:
                constexpr char const* component() const noexcept {
                  return "failoverCandidates";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<FailoverCandidates const> failoverCandidates() const {
                return FailoverCandidates::make_shared(shared_from_this());
              }

              class ErrorNum : public StaticComponent<ErrorNum, Shard> {
               public:
                constexpr char const* component() const noexcept {
                  return "errorNum";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<ErrorNum const> errorNum() const {
                return ErrorNum::make_shared(shared_from_this());
              }

              class ErrorMessage : public StaticComponent<ErrorMessage, Shard> {
               public:
                constexpr char const* component() const noexcept {
                  return "errorMessage";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<ErrorMessage const> errorMessage() const {
                return ErrorMessage::make_shared(shared_from_this());
              }

              class Error : public StaticComponent<Error, Shard> {
               public:
                constexpr char const* component() const noexcept {
                  return "error";
                }

                using BaseType::StaticComponent;
              };

              std::shared_ptr<Error const> error() const {
                return Error::make_shared(shared_from_this());
              }
            };

            std::shared_ptr<Shard const> shard(ShardID name) const {
              return Shard::make_shared(shared_from_this(), std::move(name));
            }
          };

          std::shared_ptr<Collection const> collection(CollectionID name) const {
            return Collection::make_shared(shared_from_this(), std::move(name));
          }
        };

        std::shared_ptr<Database const> database(DatabaseID name) const {
          return Database::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<Collections const> collections() const {
        return Collections::make_shared(shared_from_this());
      }

      class Databases : public StaticComponent<Databases, Current> {
       public:
        constexpr char const* component() const noexcept { return "Databases"; }

        using BaseType::StaticComponent;

        class Database : public DynamicComponent<Database, Databases, DatabaseID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;

          class Server : public DynamicComponent<Server, Database, ServerID> {
           public:
            char const* component() const noexcept {
              return value().c_str();
            }

            using BaseType::DynamicComponent;

            class Name : public StaticComponent<Name, Server> {
             public:
              constexpr char const* component() const noexcept {
                return "name";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Name const> name() const {
              return Name::make_shared(shared_from_this());
            }

            class ErrorNum : public StaticComponent<ErrorNum, Server> {
             public:
              constexpr char const* component() const noexcept {
                return "errorNum";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<ErrorNum const> errorNum() const {
              return ErrorNum::make_shared(shared_from_this());
            }

            class Id : public StaticComponent<Id, Server> {
             public:
              constexpr char const* component() const noexcept { return "id"; }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Id const> id() const {
              return Id::make_shared(shared_from_this());
            }

            class Error : public StaticComponent<Error, Server> {
             public:
              constexpr char const* component() const noexcept {
                return "error";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<Error const> error() const {
              return Error::make_shared(shared_from_this());
            }

            class ErrorMessage : public StaticComponent<ErrorMessage, Server> {
             public:
              constexpr char const* component() const noexcept {
                return "errorMessage";
              }

              using BaseType::StaticComponent;
            };

            std::shared_ptr<ErrorMessage const> errorMessage() const {
              return ErrorMessage::make_shared(shared_from_this());
            }
          };

          std::shared_ptr<Server const> server(ServerID name) const {
            return Server::make_shared(shared_from_this(), std::move(name));
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

      class Maintenance : public StaticComponent<Maintenance, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "Maintenance"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Maintenance const> maintenance() const {
        return Maintenance::make_shared(shared_from_this());
      }

      class DbServers : public StaticComponent<DbServers, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "DBServers"; }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, DbServers, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<DbServers const> dbServers() const {
        return DbServers::make_shared(shared_from_this());
      }

      class Health : public StaticComponent<Health, Supervision> {
       public:
        constexpr char const* component() const noexcept { return "Health"; }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, Health, ServerID> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;

          class SyncTime : public StaticComponent<SyncTime, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "SyncTime";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<SyncTime const> syncTime() const {
            return SyncTime::make_shared(shared_from_this());
          }

          class Timestamp : public StaticComponent<Timestamp, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "Timestamp";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Timestamp const> timestamp() const {
            return Timestamp::make_shared(shared_from_this());
          }

          class SyncStatus : public StaticComponent<SyncStatus, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "SyncStatus";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<SyncStatus const> syncStatus() const {
            return SyncStatus::make_shared(shared_from_this());
          }

          class LastAckedTime : public StaticComponent<LastAckedTime, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "LastAckedTime";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<LastAckedTime const> lastAckedTime() const {
            return LastAckedTime::make_shared(shared_from_this());
          }

          class Host : public StaticComponent<Host, Server> {
           public:
            constexpr char const* component() const noexcept { return "Host"; }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Host const> host() const {
            return Host::make_shared(shared_from_this());
          }

          class Engine : public StaticComponent<Engine, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "Engine";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Engine const> engine() const {
            return Engine::make_shared(shared_from_this());
          }

          class Version : public StaticComponent<Version, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "Version";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Version const> version() const {
            return Version::make_shared(shared_from_this());
          }

          class Status : public StaticComponent<Status, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "Status";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<Status const> status() const {
            return Status::make_shared(shared_from_this());
          }

          class ShortName : public StaticComponent<ShortName, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "ShortName";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<ShortName const> shortName() const {
            return ShortName::make_shared(shared_from_this());
          }

          class Endpoint : public StaticComponent<Endpoint, Server> {
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

        std::shared_ptr<Server const> server(ServerID server) const {
          return Server::make_shared(shared_from_this(), std::move(server));
        }
      };

      std::shared_ptr<Health const> health() const {
        return Health::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Supervision const> supervision() const {
      return Supervision::make_shared(shared_from_this());
    }

    class Target : public StaticComponent<Target, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Target"; }

      using BaseType::StaticComponent;

      class RemovedServers : public StaticComponent<RemovedServers, Target> {
       public:
        constexpr char const* component() const noexcept { return "RemovedServers"; }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, RemovedServers, ServerID> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Server const> server(ServerID server) const {
          return Server::make_shared(shared_from_this(), std::move(server));
        }
      };

      std::shared_ptr<RemovedServers const> removedServers() const {
        return RemovedServers::make_shared(shared_from_this());
      }

      class ToDo : public StaticComponent<ToDo, Target> {
       public:
        constexpr char const* component() const noexcept { return "ToDo"; }

        using BaseType::StaticComponent;

        class Job : public DynamicComponent<Job, ToDo, std::string> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Job const> job(std::string jobId) const {
          return Job::make_shared(shared_from_this(), std::move(jobId));
        }
      };

      std::shared_ptr<ToDo const> toDo() const {
        return ToDo::make_shared(shared_from_this());
      }

      class ToBeCleanedServers : public StaticComponent<ToBeCleanedServers, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "ToBeCleanedServers";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<ToBeCleanedServers const> toBeCleanedServers() const {
        return ToBeCleanedServers::make_shared(shared_from_this());
      }

      class Pending : public StaticComponent<Pending, Target> {
       public:
        constexpr char const* component() const noexcept { return "Pending"; }

        using BaseType::StaticComponent;

        class Job : public DynamicComponent<Job, Pending, std::string> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Job const> job(std::string jobId) const {
          return Job::make_shared(shared_from_this(), std::move(jobId));
        }
      };

      std::shared_ptr<Pending const> pending() const {
        return Pending::make_shared(shared_from_this());
      }

      class NumberOfDBServers : public StaticComponent<NumberOfDBServers, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "NumberOfDBServers";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<NumberOfDBServers const> numberOfDBServers() const {
        return NumberOfDBServers::make_shared(shared_from_this());
      }

      class LatestDbServerId : public StaticComponent<LatestDbServerId, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "LatestDBServerId";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<LatestDbServerId const> latestDBServerId() const {
        return LatestDbServerId::make_shared(shared_from_this());
      }

      class Failed : public StaticComponent<Failed, Target> {
       public:
        constexpr char const* component() const noexcept { return "Failed"; }

        using BaseType::StaticComponent;

        class Job : public DynamicComponent<Job, Failed, std::string> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Job const> job(std::string jobId) const {
          return Job::make_shared(shared_from_this(), std::move(jobId));
        }
      };

      std::shared_ptr<Failed const> failed() const {
        return Failed::make_shared(shared_from_this());
      }

      class CleanedServers : public StaticComponent<CleanedServers, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "CleanedServers";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<CleanedServers const> cleanedServers() const {
        return CleanedServers::make_shared(shared_from_this());
      }

      class LatestCoordinatorId : public StaticComponent<LatestCoordinatorId, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "LatestCoordinatorId";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<LatestCoordinatorId const> latestCoordinatorId() const {
        return LatestCoordinatorId::make_shared(shared_from_this());
      }

      class MapUniqueToShortId : public StaticComponent<MapUniqueToShortId, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "MapUniqueToShortID";
        }

        using BaseType::StaticComponent;

        class Server : public DynamicComponent<Server, MapUniqueToShortId, ServerID> {
         public:
          char const* component() const noexcept {
            return value().c_str();
          }

          using BaseType::DynamicComponent;

          class TransactionId : public StaticComponent<TransactionId, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "TransactionID";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<TransactionId const> transactionID() const {
            return TransactionId::make_shared(shared_from_this());
          }

          class ShortName : public StaticComponent<ShortName, Server> {
           public:
            constexpr char const* component() const noexcept {
              return "ShortName";
            }

            using BaseType::StaticComponent;
          };

          std::shared_ptr<ShortName const> shortName() const {
            return ShortName::make_shared(shared_from_this());
          }
        };

        std::shared_ptr<Server const> server(ServerID name) const {
          return Server::make_shared(shared_from_this(), std::move(name));
        }
      };

      std::shared_ptr<MapUniqueToShortId const> mapUniqueToShortID() const {
        return MapUniqueToShortId::make_shared(shared_from_this());
      }

      class FailedServers : public StaticComponent<FailedServers, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "FailedServers";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<FailedServers const> failedServers() const {
        return FailedServers::make_shared(shared_from_this());
      }

      class MapLocalToId : public StaticComponent<MapLocalToId, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "MapLocalToID";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<MapLocalToId const> mapLocalToID() const {
        return MapLocalToId::make_shared(shared_from_this());
      }

      class NumberOfCoordinators : public StaticComponent<NumberOfCoordinators, Target> {
       public:
        constexpr char const* component() const noexcept {
          return "NumberOfCoordinators";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<NumberOfCoordinators const> numberOfCoordinators() const {
        return NumberOfCoordinators::make_shared(shared_from_this());
      }

      class Finished : public StaticComponent<Finished, Target> {
       public:
        constexpr char const* component() const noexcept { return "Finished"; }

        using BaseType::StaticComponent;

        class Job : public DynamicComponent<Job, Finished, std::string> {
         public:
          char const* component() const noexcept { return value().c_str(); }

          using BaseType::DynamicComponent;
        };

        std::shared_ptr<Job const> job(std::string jobId) const {
          return Job::make_shared(shared_from_this(), std::move(jobId));
        }
      };

      std::shared_ptr<Finished const> finished() const {
        return Finished::make_shared(shared_from_this());
      }

      class Version : public StaticComponent<Version, Target> {
       public:
        constexpr char const* component() const noexcept { return "Version"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Version const> version() const {
        return Version::make_shared(shared_from_this());
      }

      class Lock : public StaticComponent<Lock, Target> {
       public:
        constexpr char const* component() const noexcept { return "Lock"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Lock const> lock() const {
        return Lock::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Target const> target() const {
      return Target::make_shared(shared_from_this());
    }

    class SystemCollectionsCreated
        : public StaticComponent<SystemCollectionsCreated, Arango> {
     public:
      constexpr char const* component() const noexcept {
        return "SystemCollectionsCreated";
      }

      using BaseType::StaticComponent;
    };

    std::shared_ptr<SystemCollectionsCreated const> systemCollectionsCreated() const {
      return SystemCollectionsCreated::make_shared(shared_from_this());
    }

    class Sync : public StaticComponent<Sync, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Sync"; }

      using BaseType::StaticComponent;

      class UserVersion : public StaticComponent<UserVersion, Sync> {
       public:
        constexpr char const* component() const noexcept {
          return "UserVersion";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<UserVersion const> userVersion() const {
        return UserVersion::make_shared(shared_from_this());
      }

      class ServerStates : public StaticComponent<ServerStates, Sync> {
       public:
        constexpr char const* component() const noexcept {
          return "ServerStates";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<ServerStates const> serverStates() const {
        return ServerStates::make_shared(shared_from_this());
      }

      class Problems : public StaticComponent<Problems, Sync> {
       public:
        constexpr char const* component() const noexcept { return "Problems"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Problems const> problems() const {
        return Problems::make_shared(shared_from_this());
      }

      class HeartbeatIntervalMs : public StaticComponent<HeartbeatIntervalMs, Sync> {
       public:
        constexpr char const* component() const noexcept {
          return "HeartbeatIntervalMs";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<HeartbeatIntervalMs const> heartbeatIntervalMs() const {
        return HeartbeatIntervalMs::make_shared(shared_from_this());
      }

      class LatestId : public StaticComponent<LatestId, Sync> {
       public:
        constexpr char const* component() const noexcept { return "LatestID"; }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<LatestId const> latestId() const {
        return LatestId::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Sync const> sync() const {
      return Sync::make_shared(shared_from_this());
    }

    class Bootstrap : public StaticComponent<Bootstrap, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Bootstrap"; }

      using BaseType::StaticComponent;
    };

    std::shared_ptr<Bootstrap const> bootstrap() const {
      return Bootstrap::make_shared(shared_from_this());
    }

    class Cluster : public StaticComponent<Cluster, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Cluster"; }

      using BaseType::StaticComponent;
    };

    std::shared_ptr<Cluster const> cluster() const {
      return Cluster::make_shared(shared_from_this());
    }

    class Agency : public StaticComponent<Agency, Arango> {
     public:
      constexpr char const* component() const noexcept { return "Agency"; }

      using BaseType::StaticComponent;

      class Definition : public StaticComponent<Definition, Agency> {
       public:
        constexpr char const* component() const noexcept {
          return "Definition";
        }

        using BaseType::StaticComponent;
      };

      std::shared_ptr<Definition const> definition() const {
        return Definition::make_shared(shared_from_this());
      }
    };

    std::shared_ptr<Agency const> agency() const {
      return Agency::make_shared(shared_from_this());
    }

    class InitDone : public StaticComponent<InitDone, Arango> {
     public:
      constexpr char const* component() const noexcept { return "InitDone"; }

      using BaseType::StaticComponent;
    };

    std::shared_ptr<InitDone const> initDone() const {
      return InitDone::make_shared(shared_from_this());
    }
  };

  std::shared_ptr<Arango const> arango() const {
    return Arango::make_shared(shared_from_this());
  }

 private:
  // May only be constructed by root()
  friend auto root() -> std::shared_ptr<Root const>;
  Root() = default;
  static auto make_shared() -> std::shared_ptr<Root const> {
    struct ConstructibleRoot : public Root {
     public:
      explicit ConstructibleRoot() noexcept = default;
    };
    return std::make_shared<ConstructibleRoot const>();
  }
};

namespace aliases {

auto arango() -> std::shared_ptr<Root::Arango const>;
auto plan() -> std::shared_ptr<Root::Arango::Plan const>;
auto current() -> std::shared_ptr<Root::Arango::Current const>;
auto target() -> std::shared_ptr<Root::Arango::Target const>;
auto supervision() -> std::shared_ptr<Root::Arango::Supervision const>;

}  // namespace aliases

}  // namespace arangodb::cluster::paths

#endif  // ARANGOD_CLUSTER_AGENCYPATHS_H
