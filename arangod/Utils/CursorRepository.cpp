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

#include "Aql/QueryCursor.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace {
bool authorized(std::pair<arangodb::Cursor*, std::string> const& cursor) {
  auto context = arangodb::ExecContext::CURRENT;
  if (context == nullptr || !arangodb::ExecContext::isAuthEnabled()) {
    return true;
  }

  if (context->isSuperuser()) {
    return true;
  }

  return (cursor.second == context->user());
}
}  // namespace

using namespace arangodb;

size_t const CursorRepository::MaxCollectCount = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor repository
////////////////////////////////////////////////////////////////////////////////

CursorRepository::CursorRepository(TRI_vocbase_t& vocbase)
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
      LOG_TOPIC(INFO, arangodb::Logger::FIXME)
          << "waiting for used cursors to become unused";
    } else if (tries == 120) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "giving up waiting for unused cursors";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ++tries;
  }

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it : _cursors) {
      delete it.second.first;
    }

    _cursors.clear();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a cursor in the registry
/// the repository will take ownership of the cursor
////////////////////////////////////////////////////////////////////////////////

Cursor* CursorRepository::addCursor(std::unique_ptr<Cursor> cursor) {
  TRI_ASSERT(cursor != nullptr);
  TRI_ASSERT(cursor->isUsed());

  CursorId const id = cursor->id();
  std::string user = ExecContext::CURRENT ? ExecContext::CURRENT->user() : "";

  {
    MUTEX_LOCKER(mutexLocker, _lock);
    _cursors.emplace(id, std::make_pair(cursor.get(), user));
  }

  return cursor.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cursor and stores it in the registry
/// the cursor will be returned with the usage flag set to true. it must be
/// returned later using release()
/// the cursor will take ownership and retain the entire QueryResult object
////////////////////////////////////////////////////////////////////////////////

Cursor* CursorRepository::createFromQueryResult(aql::QueryResult&& result, size_t batchSize,
                                                double ttl, bool hasCount) {
  TRI_ASSERT(result.result != nullptr);

  CursorId const id = TRI_NewServerSpecificTick();  // embedded server id
  TRI_ASSERT(id != 0);
  std::unique_ptr<Cursor> cursor(
      new aql::QueryResultCursor(_vocbase, id, std::move(result), batchSize, ttl, hasCount));
  cursor->use();

  return addCursor(std::move(cursor));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief creates a cursor and stores it in the registry
/// the cursor will be returned with the usage flag set to true. it must be
/// returned later using release()
/// the cursor will create a query internally and retain it until deleted
//////////////////////////////////////////////////////////////////////////////

Cursor* CursorRepository::createQueryStream(std::string const& query,
                                            std::shared_ptr<VPackBuilder> const& binds,
                                            std::shared_ptr<VPackBuilder> const& opts,
                                            size_t batchSize, double ttl,
                                            bool contextOwnedByExterior) {
  TRI_ASSERT(!query.empty());

  CursorId const id = TRI_NewServerSpecificTick();  // embedded server id
  TRI_ASSERT(id != 0);
  auto cursor = std::make_unique<aql::QueryStreamCursor>(_vocbase, id, query, binds,
                                                         opts, batchSize, ttl,
                                                         contextOwnedByExterior);
  cursor->use();

  return addCursor(std::move(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a cursor by id
////////////////////////////////////////////////////////////////////////////////

bool CursorRepository::remove(CursorId id, Cursor::CursorType type) {
  arangodb::Cursor* cursor = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _cursors.find(id);
    if (it == _cursors.end() || !::authorized(it->second)) {
      // not found
      return false;
    }

    cursor = (*it).second.first;

    if (cursor->isDeleted()) {
      // already deleted
      return false;
    }

    if (cursor->type() != type) {
      // wrong type
      return false;
    }

    if (cursor->isUsed()) {
      // cursor is in use by someone else. now mark as deleted
      cursor->setDeleted();
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

Cursor* CursorRepository::find(CursorId id, Cursor::CursorType type, bool& busy) {
  arangodb::Cursor* cursor = nullptr;
  busy = false;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _cursors.find(id);
    if (it == _cursors.end() || !::authorized(it->second)) {
      // not found
      return nullptr;
    }

    cursor = (*it).second.first;

    if (cursor->isDeleted()) {
      // already deleted
      return nullptr;
    }

    if (cursor->type() != type) {
      // wrong cursor type
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
    if (it.second.first->isUsed()) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a garbage collection on the cursors
////////////////////////////////////////////////////////////////////////////////

bool CursorRepository::garbageCollect(bool force) {
  auto const now = TRI_microtime();
  std::vector<arangodb::Cursor*> found;

  try {
    found.reserve(MaxCollectCount);

    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it = _cursors.begin(); it != _cursors.end(); /* no hoisting */) {
      auto cursor = (*it).second.first;

      if (cursor->isUsed()) {
        // must not destroy used cursors
        ++it;
        continue;
      }

      if (force || cursor->expires() < now) {
        cursor->kill();
        cursor->setDeleted();
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
  } catch (...) {
    // go on and remove whatever we found so far
  }

  // remove cursors outside the lock
  for (auto it : found) {
    delete it;
  }

  return (!found.empty());
}
