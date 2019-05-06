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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesDitch.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesDatafile.h"

using namespace arangodb;

MMFilesDitch::MMFilesDitch(MMFilesDitches* ditches, char const* filename, int line)
    : _ditches(ditches), _prev(nullptr), _next(nullptr), _filename(filename), _line(line) {}

MMFilesDitch::~MMFilesDitch() {}

/// @brief return the associated collection
LogicalCollection* MMFilesDitch::collection() const {
  return _ditches->collection();
}

MMFilesDocumentDitch::MMFilesDocumentDitch(MMFilesDitches* ditches, bool usedByTransaction,
                                           char const* filename, int line)
    : MMFilesDitch(ditches, filename, line), _usedByTransaction(usedByTransaction) {}

MMFilesDocumentDitch::~MMFilesDocumentDitch() {}

MMFilesReplicationDitch::MMFilesReplicationDitch(MMFilesDitches* ditches,
                                                 char const* filename, int line)
    : MMFilesDitch(ditches, filename, line) {}

MMFilesReplicationDitch::~MMFilesReplicationDitch() {}

MMFilesCompactionDitch::MMFilesCompactionDitch(MMFilesDitches* ditches,
                                               char const* filename, int line)
    : MMFilesDitch(ditches, filename, line) {}

MMFilesCompactionDitch::~MMFilesCompactionDitch() {}

MMFilesDropDatafileDitch::MMFilesDropDatafileDitch(
    MMFilesDitches* ditches, MMFilesDatafile* datafile, LogicalCollection* collection,
    std::function<void(MMFilesDatafile*, LogicalCollection*)> const& callback,
    char const* filename, int line)
    : MMFilesDitch(ditches, filename, line),
      _datafile(datafile),
      _collection(collection),
      _callback(callback) {}

MMFilesDropDatafileDitch::~MMFilesDropDatafileDitch() { delete _datafile; }

MMFilesRenameDatafileDitch::MMFilesRenameDatafileDitch(
    MMFilesDitches* ditches, MMFilesDatafile* datafile,
    MMFilesDatafile* compactor, LogicalCollection* collection,
    std::function<void(MMFilesDatafile*, MMFilesDatafile*, LogicalCollection*)> const& callback,
    char const* filename, int line)
    : MMFilesDitch(ditches, filename, line),
      _datafile(datafile),
      _compactor(compactor),
      _collection(collection),
      _callback(callback) {}

MMFilesRenameDatafileDitch::~MMFilesRenameDatafileDitch() {}

MMFilesUnloadCollectionDitch::MMFilesUnloadCollectionDitch(
    MMFilesDitches* ditches, LogicalCollection* collection,
    std::function<bool(LogicalCollection*)> const& callback, char const* filename, int line)
    : MMFilesDitch(ditches, filename, line), _collection(collection), _callback(callback) {}

MMFilesUnloadCollectionDitch::~MMFilesUnloadCollectionDitch() {}

MMFilesDropCollectionDitch::MMFilesDropCollectionDitch(
    MMFilesDitches* ditches, arangodb::LogicalCollection& collection,
    std::function<bool(arangodb::LogicalCollection&)> const& callback,
    char const* filename, int line)
    : MMFilesDitch(ditches, filename, line), _collection(collection), _callback(callback) {}

MMFilesDropCollectionDitch::~MMFilesDropCollectionDitch() {}

MMFilesDitches::MMFilesDitches(LogicalCollection* collection)
    : _collection(collection),
      _lock(),
      _begin(nullptr),
      _end(nullptr),
      _numMMFilesDocumentMMFilesDitches(0) {
  TRI_ASSERT(_collection != nullptr);
}

MMFilesDitches::~MMFilesDitches() { destroy(); }

/// @brief destroy the ditches - to be called on shutdown only
void MMFilesDitches::destroy() {
  MUTEX_LOCKER(mutexLocker, _lock);
  auto* ptr = _begin;

  while (ptr != nullptr) {
    auto* next = ptr->next();
    auto const type = ptr->type();

    if (type == MMFilesDitch::TRI_DITCH_COLLECTION_UNLOAD ||
        type == MMFilesDitch::TRI_DITCH_COLLECTION_DROP ||
        type == MMFilesDitch::TRI_DITCH_DATAFILE_DROP ||
        type == MMFilesDitch::TRI_DITCH_DATAFILE_RENAME ||
        type == MMFilesDitch::TRI_DITCH_REPLICATION ||
        type == MMFilesDitch::TRI_DITCH_COMPACTION) {
      delete ptr;
    } else if (type == MMFilesDitch::TRI_DITCH_DOCUMENT) {
      LOG_TOPIC("2899f", ERR, arangodb::Logger::ENGINES)
          << "logic error. shouldn't have document ditches on unload";
    } else {
      LOG_TOPIC("621bb", ERR, arangodb::Logger::ENGINES) << "unknown ditch type";
    }

    ptr = next;
  }
}

/// @brief return the associated collection
LogicalCollection* MMFilesDitches::collection() const { return _collection; }

/// @brief run a user-defined function under the lock
void MMFilesDitches::executeProtected(std::function<void()> callback) {
  MUTEX_LOCKER(mutexLocker, _lock);
  callback();
}

/// @brief process the first element from the list
/// the list will remain unchanged if the first element is either a
/// MMFilesDocumentDitch, a MMFilesReplicationDitch or a MMFilesCompactionDitch,
/// or if the list contains any MMFilesDocumentMMFilesDitches.
MMFilesDitch* MMFilesDitches::process(bool& popped,
                                      std::function<bool(MMFilesDitch const*)> callback) {
  popped = false;

  MUTEX_LOCKER(mutexLocker, _lock);

  auto ditch = _begin;

  if (ditch == nullptr) {
    // nothing to do
    return nullptr;
  }

  TRI_ASSERT(ditch != nullptr);

  auto const type = ditch->type();

  // if it is a MMFilesDocumentDitch, it means that there is still a reference
  // held to document data in a datafile. We must then not unload or remove a
  // file
  if (type == MMFilesDitch::TRI_DITCH_DOCUMENT || type == MMFilesDitch::TRI_DITCH_REPLICATION ||
      type == MMFilesDitch::TRI_DITCH_COMPACTION || _numMMFilesDocumentMMFilesDitches > 0) {
    // did not find anything at the head of the barrier list or found an element
    // marker
    // this means we must exit and cannot throw away datafiles and can unload
    // collections
    return nullptr;
  }

  // no MMFilesDocumentDitch at the head of the ditches list. This means that
  // there is some other action we can perform (i.e. unloading a datafile or a
  // collection)

  // note that there is no need to check the entire list for a
  // MMFilesDocumentDitch as the list is filled up in chronological order. New
  // ditches are always added to the tail of the list, and if we have the
  // following list HEAD -> TRI_DITCH_DATAFILE_CALLBACK -> TRI_DITCH_DOCUMENT
  // then it is still safe to execute the datafile callback operation, even if
  // there
  // is a TRI_DITCH_DOCUMENT after it.
  // This is the case because the TRI_DITCH_DATAFILE_CALLBACK is only put into
  // the
  // ditches list after changing the pointers in all headers. After the pointers
  // are
  // changed, it is safe to unload/remove an old datafile (that noone points
  // to). And
  // any newer TRI_DITCH_DOCUMENTs will always reference data inside other
  // datafiles.

  if (!callback(ditch)) {
    return ditch;
  }

  // found an element to go on with - now unlink the element from the list
  unlink(ditch);
  popped = true;

  return ditch;
}

/// @brief return the type name of the ditch at the head of the active ditches
char const* MMFilesDitches::head() {
  MUTEX_LOCKER(mutexLocker, _lock);

  auto ditch = _begin;

  if (ditch == nullptr) {
    return nullptr;
  }
  return ditch->typeName();
}

/// @brief return the number of document ditches active
uint64_t MMFilesDitches::numMMFilesDocumentMMFilesDitches() {
  MUTEX_LOCKER(mutexLocker, _lock);

  return _numMMFilesDocumentMMFilesDitches;
}

/// @brief check whether the ditches contain a ditch of a certain type
bool MMFilesDitches::contains(MMFilesDitch::DitchType type) {
  MUTEX_LOCKER(mutexLocker, _lock);

  if (type == MMFilesDitch::TRI_DITCH_DOCUMENT) {
    // shortcut
    return (_numMMFilesDocumentMMFilesDitches > 0);
  }

  auto const* ptr = _begin;

  while (ptr != nullptr) {
    if (ptr->type() == type) {
      return true;
    }

    ptr = ptr->_next;
  }

  return false;
}

/// @brief removes and frees a ditch
void MMFilesDitches::freeDitch(MMFilesDitch* ditch) {
  TRI_ASSERT(ditch != nullptr);

  bool const isMMFilesDocumentDitch = (ditch->type() == MMFilesDitch::TRI_DITCH_DOCUMENT);

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    unlink(ditch);

    if (isMMFilesDocumentDitch) {
      // decrease counter
      --_numMMFilesDocumentMMFilesDitches;
    }
  }

  delete ditch;
}

/// @brief removes and frees a ditch
/// this is used for ditches used by transactions or by externals to protect
/// the flags by the lock
void MMFilesDitches::freeMMFilesDocumentDitch(MMFilesDocumentDitch* ditch, bool fromTransaction) {
  TRI_ASSERT(ditch != nullptr);

  // First see who might still be using the ditch:
  if (fromTransaction) {
    TRI_ASSERT(ditch->usedByTransaction() == true);
  }

  freeDitch(ditch);
}

/// @brief creates a new document ditch and links it
MMFilesDocumentDitch* MMFilesDitches::createMMFilesDocumentDitch(bool usedByTransaction,
                                                                 char const* filename,
                                                                 int line) {
  try {
    auto ditch = new MMFilesDocumentDitch(this, usedByTransaction, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

/// @brief creates a new replication ditch and links it
MMFilesReplicationDitch* MMFilesDitches::createMMFilesReplicationDitch(char const* filename,
                                                                       int line) {
  try {
    auto ditch = new MMFilesReplicationDitch(this, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

/// @brief creates a new compaction ditch and links it
MMFilesCompactionDitch* MMFilesDitches::createMMFilesCompactionDitch(char const* filename,
                                                                     int line) {
  try {
    auto ditch = new MMFilesCompactionDitch(this, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

/// @brief creates a new datafile deletion ditch
MMFilesDropDatafileDitch* MMFilesDitches::createMMFilesDropDatafileDitch(
    MMFilesDatafile* datafile, LogicalCollection* collection,
    std::function<void(MMFilesDatafile*, LogicalCollection*)> const& callback,
    char const* filename, int line) {
  try {
    auto ditch = new MMFilesDropDatafileDitch(this, datafile, collection,
                                              callback, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

/// @brief creates a new datafile rename ditch
MMFilesRenameDatafileDitch* MMFilesDitches::createMMFilesRenameDatafileDitch(
    MMFilesDatafile* datafile, MMFilesDatafile* compactor, LogicalCollection* collection,
    std::function<void(MMFilesDatafile*, MMFilesDatafile*, LogicalCollection*)> const& callback,
    char const* filename, int line) {
  try {
    auto ditch = new MMFilesRenameDatafileDitch(this, datafile, compactor, collection,
                                                callback, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

/// @brief creates a new collection unload ditch
MMFilesUnloadCollectionDitch* MMFilesDitches::createMMFilesUnloadCollectionDitch(
    LogicalCollection* collection, std::function<bool(LogicalCollection*)> const& callback,
    char const* filename, int line) {
  try {
    auto ditch =
        new MMFilesUnloadCollectionDitch(this, collection, callback, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

/// @brief creates a new datafile drop ditch
MMFilesDropCollectionDitch* MMFilesDitches::createMMFilesDropCollectionDitch(
    arangodb::LogicalCollection& collection,
    std::function<bool(arangodb::LogicalCollection&)> const& callback,
    char const* filename, int line) {
  try {
    auto ditch = new MMFilesDropCollectionDitch(this, collection, callback, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

/// @brief inserts the ditch into the linked list of ditches
void MMFilesDitches::link(MMFilesDitch* ditch) {
  TRI_ASSERT(ditch != nullptr);

  ditch->_next = nullptr;
  ditch->_prev = nullptr;

  bool const isMMFilesDocumentDitch = (ditch->type() == MMFilesDitch::TRI_DITCH_DOCUMENT);

  MUTEX_LOCKER(mutexLocker, _lock);  // FIX_MUTEX

  // empty list
  if (_end == nullptr) {
    _begin = ditch;
  }

  // add to the end
  else {
    ditch->_prev = _end;
    _end->_next = ditch;
  }

  _end = ditch;

  if (isMMFilesDocumentDitch) {
    // increase counter
    ++_numMMFilesDocumentMMFilesDitches;
  }
}

/// @brief unlinks the ditch from the linked list of ditches
void MMFilesDitches::unlink(MMFilesDitch* ditch) {
  // ditch is at the beginning of the chain
  if (ditch->_prev == nullptr) {
    _begin = ditch->_next;
  } else {
    ditch->_prev->_next = ditch->_next;
  }

  // ditch is at the end of the chain
  if (ditch->_next == nullptr) {
    _end = ditch->_prev;
  } else {
    ditch->_next->_prev = ditch->_prev;
  }
}
