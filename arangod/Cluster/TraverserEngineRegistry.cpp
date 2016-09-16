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

#include "TraverserEngineRegistry.h"

#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Cluster/TraverserEngine.h"
#include "VocBase/ticks.h"

using namespace arangodb::traverser;

#ifndef USE_ENTERPRISE
TraverserEngineRegistry::EngineInfo::EngineInfo(TRI_vocbase_t* vocbase,
                                                VPackSlice info)
    : _isInUse(false),
      _engine(new TraverserEngine(vocbase, info)),
      _timeToLive(0),
      _expires(0) {}

TraverserEngineRegistry::EngineInfo::~EngineInfo() {
}
#endif

TraverserEngineRegistry::~TraverserEngineRegistry() {
  WRITE_LOCKER(writeLocker, _lock);
  for (auto const& it : _engines) {
    destroy(it.first, false);
  }
}

/// @brief Create a new Engine and return it's id
TraverserEngineID TraverserEngineRegistry::createNew(TRI_vocbase_t* vocbase,
                                                     VPackSlice engineInfo) {
  TraverserEngineID id = TRI_NewTickServer();
  TRI_ASSERT(id != 0);
  auto info = std::make_unique<EngineInfo>(vocbase, engineInfo);

  WRITE_LOCKER(writeLocker, _lock);
  TRI_ASSERT(_engines.find(id) == _engines.end());
  _engines.emplace(id, info.get());
  info.release();
  return id;
}

/// @brief Destroy the engine with the given id
void TraverserEngineRegistry::destroy(TraverserEngineID id) {
  destroy(id, true);
}

/// @brief Get the engine with the given id
BaseTraverserEngine* TraverserEngineRegistry::get(TraverserEngineID id) {
  WRITE_LOCKER(writeLocker, _lock);
  auto e = _engines.find(id);
  if (e == _engines.end()) {
    // Nothing to hand out
    // TODO: Should we throw an error instead?
    return nullptr;
  }
  if (e->second->_isInUse) {
    // Someone is still working with this engine.
    // TODO can we just delete it? Or throw an error?
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEADLOCK);
  }
  e->second->_isInUse = true; 
  return e->second->_engine.get();
}

/// @brief Returns the engine to the registry. Someone else can now use it.
void TraverserEngineRegistry::returnEngine(TraverserEngineID id) {
  WRITE_LOCKER(writeLocker, _lock);
  auto e = _engines.find(id);
  if (e == _engines.end()) {
    // Nothing to return
    // TODO: Should we throw an error instead?
    return;
  }
  if (e->second->_isInUse) {
    e->second->_isInUse = false;
  }
  // TODO Should we throw an error if we are not allowed to return this
}

/// @brief Destroy the engine with the given id, worker function
void TraverserEngineRegistry::destroy(TraverserEngineID id, bool doLock) {
  EngineInfo* engine = nullptr;

  {
    CONDITIONAL_WRITE_LOCKER(writeLocker, _lock, doLock);
    auto e = _engines.find(id);
    if (e == _engines.end()) {
      // Nothing to destroy
      // TODO: Should we throw an error instead?
      return;
    }
    // TODO what about shard locking?
    // TODO what about multiple dbs?
    if (e->second->_isInUse) {
      // Someone is still working with this engine.
      // TODO can we just delete it? Or throw an error?
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEADLOCK);
    }

    engine = e->second;
    _engines.erase(id);
  }

  delete engine;
}
