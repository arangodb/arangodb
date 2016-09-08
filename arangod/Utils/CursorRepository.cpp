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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CursorRepository.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"
#include "Utils/CollectionExport.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

size_t const CursorRepository::MaxCollectCount = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor repository
////////////////////////////////////////////////////////////////////////////////

CursorRepository::CursorRepository(TRI_vocbase_t* vocbase)
    : _vocbase(vocbase), _lock(), _cursors() {
  _cursors.reserve(64);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cursor repository
////////////////////////////////////////////////////////////////////////////////

CursorRepository::~CursorRepository() {
  try {
    garbageCollect(true);
  } catch (...) {
  }

  // wait until all used cursors have vanished
  int tries = 0;

  while (true) {
    if (!containsUsedCursor()) {
      break;
    }

    if (tries == 0) {
      LOG(INFO) << "waiting for used cursors to become unused";
    } else if (tries == 120) {
      LOG(WARN) << "giving up waiting for unused cursors";
    }

    usleep(500000);
    ++tries;
  }

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it : _cursors) {
      delete it.second;
    }

    _cursors.clear();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cursor and stores it in the registry
/// the cursor will be returned with the usage flag set to true. it must be
/// returned later using release()
/// the cursor will take ownership of both json and extra
////////////////////////////////////////////////////////////////////////////////

VelocyPackCursor* CursorRepository::createFromQueryResult(
    aql::QueryResult&& result, size_t batchSize, std::shared_ptr<VPackBuilder> extra,
    double ttl, bool count) {
  TRI_ASSERT(result.result != nullptr);

  CursorId const id = TRI_NewTickServer();

  arangodb::VelocyPackCursor* cursor = new arangodb::VelocyPackCursor(
      _vocbase, id, std::move(result), batchSize, extra, ttl, count);
  cursor->use();

  try {
    MUTEX_LOCKER(mutexLocker, _lock);
    _cursors.emplace(std::make_pair(id, cursor));
    return cursor;
  } catch (...) {
    delete cursor;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cursor and stores it in the registry
////////////////////////////////////////////////////////////////////////////////

ExportCursor* CursorRepository::createFromExport(arangodb::CollectionExport* ex,
                                                 size_t batchSize, double ttl,
                                                 bool count) {
  TRI_ASSERT(ex != nullptr);

  CursorId const id = TRI_NewTickServer();
  arangodb::ExportCursor* cursor =
      new arangodb::ExportCursor(_vocbase, id, ex, batchSize, ttl, count);

  cursor->use();

  try {
    MUTEX_LOCKER(mutexLocker, _lock);
    _cursors.emplace(std::make_pair(id, cursor));
    return cursor;
  } catch (...) {
    delete cursor;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a cursor by id
////////////////////////////////////////////////////////////////////////////////

bool CursorRepository::remove(CursorId id) {
  arangodb::Cursor* cursor = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _cursors.find(id);
    if (it == _cursors.end()) {
      // not found
      return false;
    }

    cursor = (*it).second;

    if (cursor->isDeleted()) {
      // already deleted
      return false;
    }

    if (cursor->isUsed()) {
      // cursor is in use by someone else. now mark as deleted
      cursor->deleted();
      return true;
    }

    // cursor not in use by someone else
    _cursors.erase(it);
  }

  TRI_ASSERT(cursor != nullptr);

  delete cursor;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an existing cursor by id
/// if found, the cursor will be returned with the usage flag set to true.
/// it must be returned later using release()
////////////////////////////////////////////////////////////////////////////////

Cursor* CursorRepository::find(CursorId id, bool& busy) {
  arangodb::Cursor* cursor = nullptr;
  busy = false;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _cursors.find(id);
    if (it == _cursors.end()) {
      // not found
      return nullptr;
    }

    cursor = (*it).second;

    if (cursor->isDeleted()) {
      // already deleted
      return nullptr;
    }

    if (cursor->isUsed()) {
      busy = true;
      return nullptr;
    }

    cursor->use();
  }

  return cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a cursor
////////////////////////////////////////////////////////////////////////////////

void CursorRepository::release(Cursor* cursor) {
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    TRI_ASSERT(cursor->isUsed());
    cursor->release();

    if (!cursor->isDeleted()) {
      return;
    }

    // remove from the list
    _cursors.erase(cursor->id());
  }

  // and free the cursor
  delete cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the repository contains a used cursor
////////////////////////////////////////////////////////////////////////////////

bool CursorRepository::containsUsedCursor() {
  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto it : _cursors) {
    if (it.second->isUsed()) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a garbage collection on the cursors
////////////////////////////////////////////////////////////////////////////////

bool CursorRepository::garbageCollect(bool force) {
  std::vector<arangodb::Cursor*> found;
  found.reserve(MaxCollectCount);

  auto const now = TRI_microtime();

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it = _cursors.begin(); it != _cursors.end(); /* no hoisting */) {
      auto cursor = (*it).second;

      if (cursor->isUsed()) {
        // must not destroy used cursors
        ++it;
        continue;
      }

      if (force || cursor->expires() < now) {
        cursor->deleted();
      }

      if (cursor->isDeleted()) {
        try {
          found.emplace_back(cursor);
          it = _cursors.erase(it);
        } catch (...) {
          // stop iteration
          break;
        }

        if (!force && found.size() >= MaxCollectCount) {
          break;
        }
      } else {
        ++it;
      }
    }
  }

  // remove cursors outside the lock
  for (auto it : found) {
    delete it;
  }

  return (!found.empty());
}
