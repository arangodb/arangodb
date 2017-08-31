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
                               Manager* manager, std::shared_ptr<Cache> cache)
    : _environment(environment), _manager(manager), _cache(cache) {}

FreeMemoryTask::~FreeMemoryTask() {}

bool FreeMemoryTask::dispatch() {
  _manager->prepareTask(_environment);
  auto self = shared_from_this();
  return _manager->post([self, this]() -> void { run(); });
}

void FreeMemoryTask::run() {
  bool ran = _cache->freeMemory();

  if (ran) {
    _manager->_lock.writeLock();
    Metadata* metadata = _cache->metadata();
    metadata->writeLock();
    uint64_t reclaimed = metadata->hardUsageLimit - metadata->softUsageLimit;
    metadata->adjustLimits(metadata->softUsageLimit, metadata->softUsageLimit);
    metadata->toggleResizing();
    metadata->writeUnlock();
    _manager->_globalAllocation -= reclaimed;
    _manager->_lock.writeUnlock();
  }

  _manager->unprepareTask(_environment);
}

MigrateTask::MigrateTask(Manager::TaskEnvironment environment, Manager* manager,
                         std::shared_ptr<Cache> cache,
                         std::shared_ptr<Table> table)
    : _environment(environment),
      _manager(manager),
      _cache(cache),
      _table(table) {}

MigrateTask::~MigrateTask() {}

bool MigrateTask::dispatch() {
  _manager->prepareTask(_environment);
  auto self = shared_from_this();
  return _manager->post([self, this]() -> void { run(); });
}

void MigrateTask::run() {
  // do the actual migration
  bool ran = _cache->migrate(_table);

  if (!ran) {
    Metadata* metadata = _cache->metadata();
    metadata->writeLock();
    metadata->toggleMigrating();
    metadata->writeUnlock();
    _manager->reclaimTable(_table);
  }

  _manager->unprepareTask(_environment);
}
