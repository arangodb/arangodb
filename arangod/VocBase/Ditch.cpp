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

#include "Ditch.h"
#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "VocBase/document-collection.h"

using namespace arangodb;

Ditch::Ditch(Ditches* ditches, char const* filename, int line)
    : _ditches(ditches),
      _prev(nullptr),
      _next(nullptr),
      _filename(filename),
      _line(line) {}

Ditch::~Ditch() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the associated collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* Ditch::collection() const {
  return _ditches->collection();
}

DocumentDitch::DocumentDitch(Ditches* ditches, bool usedByTransaction,
                             char const* filename, int line)
    : Ditch(ditches, filename, line),
      _usedByExternal(0),
      _usedByTransaction(usedByTransaction) {}

DocumentDitch::~DocumentDitch() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the _usedByTransaction flag, using the required lock
////////////////////////////////////////////////////////////////////////////////

void DocumentDitch::setUsedByTransaction() {
  auto callback = [this]() -> void { _usedByTransaction = true; };
  _ditches->executeProtected(callback);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increases the _usedByExternal value, using the required lock
////////////////////////////////////////////////////////////////////////////////

void DocumentDitch::setUsedByExternal() {
  auto callback = [this]() -> void { ++_usedByExternal; };
  _ditches->executeProtected(callback);
}

ReplicationDitch::ReplicationDitch(Ditches* ditches, char const* filename,
                                   int line)
    : Ditch(ditches, filename, line) {}

ReplicationDitch::~ReplicationDitch() {}

CompactionDitch::CompactionDitch(Ditches* ditches, char const* filename,
                                 int line)
    : Ditch(ditches, filename, line) {}

CompactionDitch::~CompactionDitch() {}

DropDatafileDitch::DropDatafileDitch(
    Ditches* ditches, TRI_datafile_t* datafile, void* data,
    std::function<void(TRI_datafile_t*, void*)> callback, char const* filename,
    int line)
    : Ditch(ditches, filename, line),
      _datafile(datafile),
      _data(data),
      _callback(callback) {}

DropDatafileDitch::~DropDatafileDitch() {}

RenameDatafileDitch::RenameDatafileDitch(
    Ditches* ditches, TRI_datafile_t* datafile, void* data,
    std::function<void(TRI_datafile_t*, void*)> callback, char const* filename,
    int line)
    : Ditch(ditches, filename, line),
      _datafile(datafile),
      _data(data),
      _callback(callback) {}

RenameDatafileDitch::~RenameDatafileDitch() {}

UnloadCollectionDitch::UnloadCollectionDitch(
    Ditches* ditches, TRI_collection_t* collection, void* data,
    std::function<bool(TRI_collection_t*, void*)> callback,
    char const* filename, int line)
    : Ditch(ditches, filename, line),
      _collection(collection),
      _data(data),
      _callback(callback) {}

UnloadCollectionDitch::~UnloadCollectionDitch() {}

DropCollectionDitch::DropCollectionDitch(
    Ditches* ditches, TRI_collection_t* collection, void* data,
    std::function<bool(TRI_collection_t*, void*)> callback,
    char const* filename, int line)
    : Ditch(ditches, filename, line),
      _collection(collection),
      _data(data),
      _callback(callback) {}

DropCollectionDitch::~DropCollectionDitch() {}

Ditches::Ditches(TRI_document_collection_t* collection)
    : _collection(collection),
      _lock(),
      _begin(nullptr),
      _end(nullptr),
      _numDocumentDitches(0) {
  TRI_ASSERT(_collection != nullptr);
}

Ditches::~Ditches() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the ditches - to be called on shutdown only
////////////////////////////////////////////////////////////////////////////////

void Ditches::destroy() {
  auto* ptr = _begin;

  while (ptr != nullptr) {
    auto* next = ptr->next();
    auto const type = ptr->type();

    if (type == Ditch::TRI_DITCH_COLLECTION_UNLOAD ||
        type == Ditch::TRI_DITCH_COLLECTION_DROP ||
        type == Ditch::TRI_DITCH_DATAFILE_DROP ||
        type == Ditch::TRI_DITCH_DATAFILE_RENAME ||
        type == Ditch::TRI_DITCH_REPLICATION ||
        type == Ditch::TRI_DITCH_COMPACTION) {
      delete ptr;
    } else if (type == Ditch::TRI_DITCH_DOCUMENT) {
      LOG(ERR) << "logic error. shouldn't have document ditches on unload";
    } else {
      LOG(ERR) << "unknown ditch type";
    }

    ptr = next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the associated collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* Ditches::collection() const { return _collection; }

////////////////////////////////////////////////////////////////////////////////
/// @brief run a user-defined function under the lock
////////////////////////////////////////////////////////////////////////////////

void Ditches::executeProtected(std::function<void()> callback) {
  MUTEX_LOCKER(mutexLocker, _lock);
  callback();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the first element from the list
/// the list will remain unchanged if the first element is either a
/// DocumentDitch, a ReplicationDitch or a CompactionDitch, or if the list
/// contains any DocumentDitches.
////////////////////////////////////////////////////////////////////////////////

Ditch* Ditches::process(bool& popped,
                        std::function<bool(Ditch const*)> callback) {
  popped = false;

  MUTEX_LOCKER(mutexLocker, _lock);

  auto ditch = _begin;

  if (ditch == nullptr) {
    // nothing to do
    return nullptr;
  }

  TRI_ASSERT(ditch != nullptr);

  auto const type = ditch->type();

  // if it is a DocumentDitch, it means that there is still a reference held
  // to document data in a datafile. We must then not unload or remove a file
  if (type == Ditch::TRI_DITCH_DOCUMENT ||
      type == Ditch::TRI_DITCH_REPLICATION ||
      type == Ditch::TRI_DITCH_COMPACTION || _numDocumentDitches > 0) {
    // did not find anything at the head of the barrier list or found an element
    // marker
    // this means we must exit and cannot throw away datafiles and can unload
    // collections
    return nullptr;
  }

  // no DocumentDitch at the head of the ditches list. This means that there is
  // some other action we can perform (i.e. unloading a datafile or a
  // collection)

  // note that there is no need to check the entire list for a DocumentDitch as
  // the list is filled up in chronological order. New ditches are always added
  // to the
  // tail of the list, and if we have the following list
  // HEAD -> TRI_DITCH_DATAFILE_CALLBACK -> TRI_DITCH_DOCUMENT
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

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type name of the ditch at the head of the active ditches
////////////////////////////////////////////////////////////////////////////////

char const* Ditches::head() {
  MUTEX_LOCKER(mutexLocker, _lock);

  auto ditch = _begin;

  if (ditch == nullptr) {
    return nullptr;
  }
  return ditch->typeName();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of document ditches active
////////////////////////////////////////////////////////////////////////////////

uint64_t Ditches::numDocumentDitches() {
  MUTEX_LOCKER(mutexLocker, _lock);

  return _numDocumentDitches;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the ditches contain a ditch of a certain type
////////////////////////////////////////////////////////////////////////////////

bool Ditches::contains(Ditch::DitchType type) {
  MUTEX_LOCKER(mutexLocker, _lock);

  if (type == Ditch::TRI_DITCH_DOCUMENT) {
    // shortcut
    return (_numDocumentDitches > 0);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief removes and frees a ditch
////////////////////////////////////////////////////////////////////////////////

void Ditches::freeDitch(Ditch* ditch) {
  TRI_ASSERT(ditch != nullptr);

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    unlink(ditch);

    if (ditch->type() == Ditch::TRI_DITCH_DOCUMENT) {
      // decrease counter
      --_numDocumentDitches;
    }
  }

  delete ditch;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes and frees a ditch
/// this is used for ditches used by transactions or by externals to protect
/// the flags by the lock
////////////////////////////////////////////////////////////////////////////////

void Ditches::freeDocumentDitch(DocumentDitch* ditch, bool fromTransaction) {
  TRI_ASSERT(ditch != nullptr);

  bool shouldFree = false;
  {
    MUTEX_LOCKER(mutexLocker, _lock);  // FIX_MUTEX

    // First see who might still be using the ditch:
    if (fromTransaction) {
      TRI_ASSERT(ditch->_usedByTransaction == true);
      ditch->_usedByTransaction = false;
    } else {
      // note: _usedByExternal may or may not be set when we get here
      // the reason is that there are ditches not linked at all
      // (when a ditch is created ahead of operations but the operations are
      // not executed etc.)
      if (ditch->_usedByExternal > 0) {
        --ditch->_usedByExternal;
      }
    }

    if (ditch->_usedByTransaction == false && ditch->_usedByExternal == 0) {
      // Really free it:

      unlink(ditch);

      // decrease counter
      --_numDocumentDitches;

      // free the ditch
      shouldFree = true;
    }
  }

  if (shouldFree) {
    delete ditch;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document ditch and links it
////////////////////////////////////////////////////////////////////////////////

DocumentDitch* Ditches::createDocumentDitch(bool usedByTransaction,
                                            char const* filename, int line) {
  try {
    auto ditch = new DocumentDitch(this, usedByTransaction, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new replication ditch and links it
////////////////////////////////////////////////////////////////////////////////

ReplicationDitch* Ditches::createReplicationDitch(char const* filename,
                                                  int line) {
  try {
    auto ditch = new ReplicationDitch(this, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compaction ditch and links it
////////////////////////////////////////////////////////////////////////////////

CompactionDitch* Ditches::createCompactionDitch(char const* filename,
                                                int line) {
  try {
    auto ditch = new CompactionDitch(this, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile deletion ditch
////////////////////////////////////////////////////////////////////////////////

DropDatafileDitch* Ditches::createDropDatafileDitch(
    TRI_datafile_t* datafile, void* data,
    std::function<void(struct TRI_datafile_t*, void*)> callback,
    char const* filename, int line) {
  try {
    auto ditch =
        new DropDatafileDitch(this, datafile, data, callback, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile rename ditch
////////////////////////////////////////////////////////////////////////////////

RenameDatafileDitch* Ditches::createRenameDatafileDitch(
    TRI_datafile_t* datafile, void* data,
    std::function<void(struct TRI_datafile_t*, void*)> callback,
    char const* filename, int line) {
  try {
    auto ditch =
        new RenameDatafileDitch(this, datafile, data, callback, filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection unload ditch
////////////////////////////////////////////////////////////////////////////////

UnloadCollectionDitch* Ditches::createUnloadCollectionDitch(
    TRI_collection_t* collection, void* data,
    std::function<bool(TRI_collection_t*, void*)> callback,
    char const* filename, int line) {
  try {
    auto ditch = new UnloadCollectionDitch(this, collection, data, callback,
                                           filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile drop ditch
////////////////////////////////////////////////////////////////////////////////

DropCollectionDitch* Ditches::createDropCollectionDitch(
    TRI_collection_t* collection, void* data,
    std::function<bool(TRI_collection_t*, void*)> callback,
    char const* filename, int line) {
  try {
    auto ditch = new DropCollectionDitch(this, collection, data, callback,
                                         filename, line);
    link(ditch);

    return ditch;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts the ditch into the linked list of ditches
////////////////////////////////////////////////////////////////////////////////

void Ditches::link(Ditch* ditch) {
  TRI_ASSERT(ditch != nullptr);

  MUTEX_LOCKER(mutexLocker, _lock);  // FIX_MUTEX

  // empty list
  if (_end == nullptr) {
    ditch->_next = nullptr;
    ditch->_prev = nullptr;

    _begin = ditch;
    _end = ditch;
  }

  // add to the end
  else {
    ditch->_next = nullptr;
    ditch->_prev = _end;

    _end->_next = ditch;
    _end = ditch;
  }

  if (ditch->type() == Ditch::TRI_DITCH_DOCUMENT) {
    // increase counter
    ++_numDocumentDitches;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks the ditch from the linked list of ditches
////////////////////////////////////////////////////////////////////////////////

void Ditches::unlink(Ditch* ditch) {
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
