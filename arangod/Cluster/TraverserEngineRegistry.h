////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace traverser {

class TraverserEngine;

/// @brief type of a Traverser Engine Id
typedef TRI_voc_tick_t TraverserEngineID;

class TraverserEngineRegistry {
 public:
  TraverserEngineRegistry() {}

  ~TraverserEngineRegistry();

  /// @brief Create a new Engine in the registry.
  ///        It can be referred to by the returned
  ///        ID. If the returned ID is 0 something
  ///        internally went wrong.
  TraverserEngineID createNew();

  /// @brief Get the engine with the given ID.
  ///        TODO Test what happens if this pointer
  ///        is requested twice in parallel?
  TraverserEngine* get(TraverserEngineID);

  /// @brief Destroys the engine with the given id.
  void destroy(TraverserEngineID);

  /// @brief Returns the engine with the given id.
  ///        NOTE: Caller is NOT allowed to use the
  ///        engine after this return.
  void returnEngine(TraverserEngineID);

 private:

  struct EngineInfo {
    bool _isInUse;            // Flag if this engine is in use
    TraverserEngine* _engine; // The real engine

    double _timeToLive;       // in seconds
    double _expires;          // UNIX UTC timestamp for expiration
  };

  /// @brief the actual map of engines
  std::unordered_map<TraverserEngineID, EngineInfo*> _engines;

  /// @brief _lock, the read/write lock for access
  basics::ReadWriteLock _lock;
};

} // namespace traverser
} // namespace arangodb
#endif
