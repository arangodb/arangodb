////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "Cache/ManagerTasks.h"
#include "Basics/Common.h"
#include "Basics/asio-helper.h"
#include "Cache/Cache.h"
#include "Cache/Manager.h"
#include "Cache/Metadata.h"

using namespace arangodb::cache;

FreeMemoryTask::FreeMemoryTask(Manager::TaskEnvironment environment,
                               Manager* manager, Manager::MetadataItr& metadata)
    : _environment(environment), _manager(manager) {
  metadata->lock();
  _cache = metadata->cache();
  metadata->unlock();
}

FreeMemoryTask::~FreeMemoryTask() {}

bool FreeMemoryTask::dispatch() {
  auto ioService = _manager->ioService();
  if (ioService == nullptr) {
    return false;
  }

  _manager->prepareTask(_environment);
  auto self = shared_from_this();
  ioService->post([self, this]() -> void { run(); });

  return true;
}

void FreeMemoryTask::run() {
  bool ran = _cache->freeMemory();

  if (ran) {
    _manager->_state.lock();
    auto metadata = _cache->metadata();
    metadata->lock();
    uint64_t reclaimed = metadata->hardLimit() - metadata->softLimit();
    metadata->adjustLimits(metadata->softLimit(), metadata->softLimit());
    metadata->toggleFlag(State::Flag::resizing);
    metadata->unlock();
    _manager->_globalAllocation -= reclaimed;
    _manager->_state.unlock();
  }

  _manager->unprepareTask(_environment);
}

MigrateTask::MigrateTask(Manager::TaskEnvironment environment, Manager* manager,
                         Manager::MetadataItr& metadata)
    : _environment(environment), _manager(manager) {
  metadata->lock();
  _cache = metadata->cache();
  metadata->unlock();
}

MigrateTask::~MigrateTask() {}

bool MigrateTask::dispatch() {
  auto ioService = _manager->ioService();
  if (ioService == nullptr) {
    return false;
  }

  _manager->prepareTask(_environment);
  auto self = shared_from_this();
  ioService->post([self, this]() -> void { run(); });

  return true;
}

void MigrateTask::run() {
  // do the actual migration
  bool ran = _cache->migrate();

  if (ran) {
    _manager->_state.lock();
    auto metadata = _cache->metadata();
    metadata->lock();
    _manager->reclaimTables(metadata, true);
    metadata->toggleFlag(State::Flag::migrating);
    metadata->unlock();
    _manager->_state.unlock();
  }

  _manager->unprepareTask(_environment);
}
