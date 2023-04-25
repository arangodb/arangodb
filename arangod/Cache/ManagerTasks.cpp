////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "ManagerTasks.h"

#include "Basics/SpinLocker.h"
#include "Cache/Cache.h"
#include "Cache/Manager.h"
#include "Cache/Metadata.h"

namespace arangodb::cache {

FreeMemoryTask::FreeMemoryTask(Manager::TaskEnvironment environment,
                               Manager& manager, std::shared_ptr<Cache> cache)
    : _environment(environment), _manager(manager), _cache(std::move(cache)) {}

FreeMemoryTask::~FreeMemoryTask() = default;

bool FreeMemoryTask::dispatch() {
  _manager.prepareTask(_environment);

  try {
    if (_manager.post([self = shared_from_this()]() -> void { self->run(); })) {
      // intentionally don't unprepare task
      return true;
    }
    _manager.unprepareTask(_environment);
    return false;
  } catch (...) {
    _manager.unprepareTask(_environment);
    throw;
  }
}

void FreeMemoryTask::run() {
  using basics::SpinLocker;

  TRI_ASSERT(_cache->isResizingFlagSet());
  bool clearResizingFlag = true;

  try {
    bool ran = _cache->freeMemory();

    // flag must still be set after freeMemory()
    TRI_ASSERT(_cache->isResizingFlagSet());

    if (ran) {
      std::uint64_t reclaimed = 0;
      SpinLocker guard(SpinLocker::Mode::Write, _manager._lock);
      Metadata& metadata = _cache->metadata();
      {
        SpinLocker metaGuard(SpinLocker::Mode::Write, metadata.lock());
        TRI_ASSERT(metadata.isResizing());
        reclaimed = metadata.hardUsageLimit - metadata.softUsageLimit;
        metadata.adjustLimits(metadata.softUsageLimit, metadata.softUsageLimit);
        metadata.toggleResizing();
        TRI_ASSERT(!metadata.isResizing());
      }

      TRI_ASSERT(_manager._globalAllocation >=
                 reclaimed + _manager._fixedAllocation);
      _manager._globalAllocation -= reclaimed;
      TRI_ASSERT(_manager._globalAllocation >= _manager._fixedAllocation);
    } else {
      // make sure we are always clearing the resizing flag
      Metadata& metadata = _cache->metadata();
      SpinLocker metaGuard(SpinLocker::Mode::Write, metadata.lock());
      TRI_ASSERT(metadata.isResizing());
      metadata.toggleResizing();
      TRI_ASSERT(!metadata.isResizing());
    }

    clearResizingFlag = false;

    _manager.unprepareTask(_environment);
  } catch (...) {
    if (clearResizingFlag) {
      // make sure we are always clearing the resizing flag
      Metadata& metadata = _cache->metadata();
      SpinLocker metaGuard(SpinLocker::Mode::Write, metadata.lock());
      TRI_ASSERT(metadata.isResizing());
      metadata.toggleResizing();
      TRI_ASSERT(!metadata.isResizing());
    }

    // always count down at the end
    _manager.unprepareTask(_environment);
    throw;
  }
}

MigrateTask::MigrateTask(Manager::TaskEnvironment environment, Manager& manager,
                         std::shared_ptr<Cache> cache,
                         std::shared_ptr<Table> table)
    : _environment(environment),
      _manager(manager),
      _cache(std::move(cache)),
      _table(std::move(table)) {}

MigrateTask::~MigrateTask() = default;

bool MigrateTask::dispatch() {
  _manager.prepareTask(_environment);

  try {
    if (_manager.post([self = shared_from_this()]() -> void { self->run(); })) {
      // intentionally don't unprepare task
      return true;
    }
    _manager.unprepareTask(_environment);
    return false;
  } catch (...) {
    _manager.unprepareTask(_environment);
    throw;
  }
}

void MigrateTask::run() {
  try {
    // we must be migrating when we get here
    TRI_ASSERT(_cache->isMigratingFlagSet());

    // do the actual migration
    bool ran = _cache->migrate(_table);

    // migrate() must have unset the migrating flag, but we
    // cannot check it here because another MigrateTask may
    // have been scheduled in the meantime and have set the
    // flag again, which would be a valid situation.

    if (!ran) {
      _manager.reclaimTable(std::move(_table), false);
      TRI_ASSERT(_table == nullptr);
    }

    _manager.unprepareTask(_environment);
  } catch (...) {
    // always count down at the end
    _manager.unprepareTask(_environment);
    throw;
  }
}
}  // namespace arangodb::cache
