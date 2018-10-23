////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_TRAVERSER_ENGINE_REGISTRY_H
#define ARANGOD_CLUSTER_TRAVERSER_ENGINE_REGISTRY_H 1

#include "Basics/ConditionVariable.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
  class Context;
}

namespace traverser {

class BaseEngine;

/// @brief type of a Traverser Engine Id
typedef TRI_voc_tick_t TraverserEngineID;

class TraverserEngineRegistry {
  friend class BaseTraverserEngine;
 public:
  TraverserEngineRegistry() {}

  TEST_VIRTUAL ~TraverserEngineRegistry();

  /// @brief Create a new Engine in the registry.
  ///        It can be referred to by the returned
  ///        ID. If the returned ID is 0 something
  ///        internally went wrong.
  TEST_VIRTUAL TraverserEngineID createNew(
    TRI_vocbase_t& vocbase,
    std::shared_ptr<transaction::Context> const& ctx,
    arangodb::velocypack::Slice engineInfo,
    double ttl,
    bool needToLock
  );

  /// @brief Get the engine with the given ID.
  ///        TODO Test what happens if this pointer
  ///        is requested twice in parallel?
  BaseEngine* get(TraverserEngineID);

  /// @brief Destroys the engine with the given id.
  void destroy(TraverserEngineID);

  /// @brief Returns the engine with the given id.
  ///        NOTE: Caller is NOT allowed to use the
  ///        engine after this return. If the ttl
  ///        is negative (the default), then the old
  ///        one is taken again.
  void returnEngine(TraverserEngineID, double ttl = -1.0);

  /// @brief expireEngines, this deletes all expired engines from the registry
  void expireEngines();

  /// @brief return number of registered engines
  size_t numberRegisteredEngines();

  /// @brief destroy all registered engines
  void destroyAll();

 private:
  
  void destroy(TraverserEngineID, bool doLock);

  struct EngineInfo {
    bool _isInUse;                                 // Flag if this engine is in use
    bool _toBeDeleted;                             // Should be deleted after
                                                   // next return
    std::unique_ptr<BaseEngine> _engine;           // The real engine

    double _timeToLive;                            // in seconds
    double _expires;                               // UNIX UTC timestamp for expiration

    EngineInfo(
      TRI_vocbase_t& vocbase,
      std::shared_ptr<transaction::Context> const& ctx,
      arangodb::velocypack::Slice info,
      bool needToLock
    );
    ~EngineInfo();
  };

  /// @brief the actual map of engines
  std::unordered_map<TraverserEngineID, EngineInfo*> _engines;

  /// @brief _lock, the read/write lock for access
  basics::ReadWriteLock _lock;

  /// @brief variable for traverser engines already in use
  basics::ConditionVariable _cv;
};

} // namespace traverser
} // namespace arangodb
#endif
