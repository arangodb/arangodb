////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBWrapper.h"

#include "Basics/MutexLocker.h"

namespace arangodb {

/// @brief wrapper for TransactionDB::Open
rocksdb::Status RocksDBWrapper::Open(const rocksdb::DBOptions& db_options,
                                     const rocksdb::TransactionDBOptions& txn_db_options,
                                     const std::string& dbname,
                                     const std::vector<rocksdb::ColumnFamilyDescriptor>& column_families,
                                     std::vector<rocksdb::ColumnFamilyHandle*>* handles,
                                     RocksDBWrapper** dbptr) {
  rocksdb::Status ret_status;
  rocksdb::TransactionDB * trans_db;

  *dbptr = nullptr;
  ret_status = TransactionDB::Open(db_options, txn_db_options, dbname, column_families, handles, &trans_db);

  if (ret_status.ok()) {
    // create copies of all the parameters to ease reuse on hot backup restore
    RocksDBWrapper * new_wrap = new RocksDBWrapper(db_options, txn_db_options, dbname,
                                                   column_families, handles, trans_db);
    *dbptr = new_wrap;
  }

  return ret_status;

} // RocksDBWrapper::Open


/// @brief construct to save all starting parameters for use in later restart
RocksDBWrapper::RocksDBWrapper(const rocksdb::DBOptions& db_options,
                                               const rocksdb::TransactionDBOptions& txn_db_options,
                                               const std::string& dbname,
                                               const std::vector<rocksdb::ColumnFamilyDescriptor>& column_families,
                               std::vector<rocksdb::ColumnFamilyHandle*>* handles, rocksdb::TransactionDB * trans)
  : TransactionDB(trans), _db_options(db_options), _txn_db_options(txn_db_options), _dbname(dbname),
    _column_families(column_families), _handlesPtr(handles), _db(trans) {

  return;

} // RocksDBWrapper::RocksDBWrapper


/// @brief start a rocksdb pause, then return.  True if pause started within timeout
bool RocksDBWrapper::pauseRocksDB(std::chrono::milliseconds timeout, bool force) {
  bool withinTimeout(false);

  // NOTE: this is intentionally without unlock protection
//  withinTimeout=_rwlock.writeLock(timeout);

  if (withinTimeout || force) {
    deactivateAllIterators();
    deactivateAllSnapshots();
    _db->Close();
    delete _db;
    _db = nullptr;
  } // if

  return withinTimeout;

} // RocksDBWrapper::pauseRocksDB


rocksdb::Iterator* RocksDBWrapper::NewIterator(const rocksdb::ReadOptions& opts,
                                               rocksdb::ColumnFamilyHandle* column_family) {
    READ_LOCKER(lock, _rwlock);
#if 1
    rocksdb::ReadOptions local_options(opts);
    local_options.snapshot = rewriteSnapshot(opts.snapshot);
    RocksDBWrapperIterator * wrapIt = new RocksDBWrapperIterator(_db->NewIterator(local_options, column_family), *this);
    return wrapIt;
#else
    return _db->NewIterator(opts, column_family);
#endif
} // RocksDBWrapper::NewIterator


/// @brief Maintain a list of outstanding iterators so they can be disabled prior to restore
void RocksDBWrapper::registerIterator(RocksDBWrapperIterator * iter) {

  MUTEX_LOCKER(lock, _iterMutex);

  if (!_iterSet.insert(iter).second) {
    TRI_ASSERT(false);
  } // if

  return;

} // RocksDBWrapper::registerIterator


/// @brief Maintain a list of outstanding iterators so they can be disabled prior to restore
void RocksDBWrapper::releaseIterator(RocksDBWrapperIterator * iter) {

  MUTEX_LOCKER(lock, _iterMutex);

  if (0 == _iterSet.erase(iter)) {
    TRI_ASSERT(false);
  } // if

  return;

} // RocksDBWrapper::releaseIterator


/// @brief Walk list of active iterators releasing the underlying object and invalidating state
void RocksDBWrapper::deactivateAllIterators() {

  // have global write lock on db, iterator lock is redundant but safe
  MUTEX_LOCKER(lock, _iterMutex);
  for (auto iter : _iterSet) {
    iter->arangoRelease();
  } // for

  return;

} // RockDBWrapper::deactivateAllIterators


  const rocksdb::Snapshot* RocksDBWrapper::GetSnapshot() {
    READ_LOCKER(lock, _rwlock);
#if 1
    RocksDBWrapperSnapshot * wrapSnap = new RocksDBWrapperSnapshot(_db->GetSnapshot(), *this);

    return wrapSnap;
#else
    return _db->GetSnapshot();
#endif
} // RocksDBWrapper::GetSnapshot

/// @brief Capital R ReleaseSnapshot is a rocksdb routine
void RocksDBWrapper::ReleaseSnapshot(const rocksdb::Snapshot * snapshot) {
    READ_LOCKER(lock, _rwlock);
#if 1
    RocksDBWrapperSnapshot * wrapper;

    wrapper = (RocksDBWrapperSnapshot *) snapshot;
    wrapper->deleteSnapshot(_db);
#else
    _db->ReleaseSnapshot(snapshot);
#endif
} // RocksDBWrapper::ReleaseSnapshot

/// @brief Maintain a list of outstanding snapshots so they can be disabled prior to restore
void RocksDBWrapper::registerSnapshot(RocksDBWrapperSnapshot * snap) {

  MUTEX_LOCKER(lock, _snapMutex);

  if (!_snapSet.insert(snap).second) {
    TRI_ASSERT(false);
  } // if

  return;

} // RocksDBWrapper::registerSnapshot


/// @brief lower case r releaseSnapshot removes known snapshot from local tracking
void RocksDBWrapper::releaseSnapshot(RocksDBWrapperSnapshot * snap) {

  MUTEX_LOCKER(lock, _snapMutex);

  if (0 == _snapSet.erase(snap)) {
    TRI_ASSERT(false);
  } // if

  return;

} // RocksDBWrapper::releaseSnapshot

const rocksdb::Snapshot * RocksDBWrapper::rewriteSnapshot(const rocksdb::Snapshot * snap) {

  MUTEX_LOCKER(lock, _snapMutex);
  const rocksdb::Snapshot * retSnap(nullptr);

  auto it = _snapSet.find((RocksDBWrapperSnapshot *)snap);
  if (_snapSet.end() != it) {
    retSnap = (*it)->getOriginalSnapshot();
  } // if

  return retSnap;

} // RocksDBWrapper::releaseSnapshot

/// @brief Walk list of active snapshots releasing the underlying object and invalidating state
void RocksDBWrapper::deactivateAllSnapshots() {

  // have global write lock on db, snapshot lock is redundant but safe
  MUTEX_LOCKER(lock, _snapMutex);
  for (auto iter : _snapSet) {
    iter->arangoRelease(_db);
  } // for

  return;

} // RockDBWrapper::deactivateAllSnapshots


}  // namespace arangodb
