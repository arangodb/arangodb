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
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"

namespace arangodb {

/// @brief wrapper for TransactionDB::Open
rocksdb::Status RocksDBWrapper::Open(const rocksdb::DBOptions& db_options,
                                     const rocksdb::TransactionDBOptions& txn_db_options,
                                     const std::string& dbname,
                                     const std::vector<rocksdb::ColumnFamilyDescriptor>& column_families,
                                     std::vector<RocksDBWrapperCFHandle*>* handles,
                                     RocksDBWrapper** dbptr) {
  rocksdb::Status ret_status;
  rocksdb::TransactionDB * trans_db;
  std::vector<rocksdb::ColumnFamilyHandle *> newHandles;

  *dbptr = nullptr;
  ret_status = rocksdb::TransactionDB::Open(db_options, txn_db_options, dbname, column_families, &newHandles, &trans_db);

  if (ret_status.ok()) {
    // create copies of all the parameters to ease reuse on hot backup restore
    RocksDBWrapper * new_wrap = new RocksDBWrapper(db_options, txn_db_options, dbname,
                                                   column_families, handles, trans_db);
    new_wrap->buildCFWrappers(newHandles);

    *dbptr = new_wrap;
  }

  return ret_status;

} // RocksDBWrapper::Open


// this is NOT a static, wrapper was previously created with Open.  now creating a new rocksdb
//  instance within this existing wrapper
rocksdb::Status RocksDBWrapper::ReOpen() {

  rocksdb::Status ret_status;
  rocksdb::TransactionDB * trans_db={nullptr};
  std::vector<rocksdb::ColumnFamilyHandle *> newHandles;
  ret_status = rocksdb::TransactionDB::Open(_db_options, _txn_db_options, _dbname, _column_families, &newHandles, &trans_db);
  _db = trans_db;

  updateCFWrappers(newHandles);
  return ret_status;

} // RocksDBWrapper::ReOpen


/// @brief construct to save all starting parameters for use in later restart
RocksDBWrapper::RocksDBWrapper(const rocksdb::DBOptions& db_options,
                                               const rocksdb::TransactionDBOptions& txn_db_options,
                                               const std::string& dbname,
                                               const std::vector<rocksdb::ColumnFamilyDescriptor>& column_families,
                               std::vector<RocksDBWrapperCFHandle*>* handles, rocksdb::TransactionDB * trans)
  : /*TransactionDB(trans),*/ _db_options(db_options), _txn_db_options(txn_db_options), _dbname(dbname),
    _column_families(column_families), _handlesPtr(handles), _db(trans) {

  return;

} // RocksDBWrapper::RocksDBWrapper


/// @brief start a rocksdb pause, then return.  True if pause started within timeout
bool RocksDBWrapper::pauseRocksDB(std::chrono::milliseconds timeout) {
  bool withinTimeout(false);

  // NOTE: this is intentionally without unlock protection
  withinTimeout=_rwlock.writeLock(timeout);

  if (withinTimeout) {
    deactivateAllIterators();
    deactivateAllSnapshots();
    rocksutils::globalRocksEngine()->shutdownRocksDBInstance(false);

    _db->Close();
    delete _db;
    _db = nullptr;
  } // if

  return withinTimeout;

} // RocksDBWrapper::pauseRocksDB


bool RocksDBWrapper::restartRocksDB(bool isRetry) {
  rocksdb::Status ret_status;

  rocksutils::globalRocksEngine()->setEventListeners();

  ret_status = rocksutils::globalRocksEngine()->callRocksDBOpen(_txn_db_options, _column_families, _handlesPtr);
    //TransactionDB::Open(_db_options, _txn_db_options, _dbname, _column_families, _handlesPtr, &_db);

  // leave this locked on a fail to give time for a retry with previous db?
  if (ret_status.ok() || isRetry) {
    _rwlock.unlockWrite();
  } // if

  return ret_status.ok();

} // RocksDBWrapper::restartRocksDB


rocksdb::Iterator* RocksDBWrapper::NewIterator(const rocksdb::ReadOptions& opts,
                                               RocksDBWrapperCFHandle * column_family) {
    READ_LOCKER(lock, _rwlock);
    rocksdb::ReadOptions local_options(opts);
    local_options.snapshot = rewriteSnapshot(opts.snapshot);
    RocksDBWrapperIterator * wrapIt = new RocksDBWrapperIterator(_db->NewIterator(local_options, column_family->unwrapCF()), *this);
    return wrapIt;

} // RocksDBWrapper::NewIterator


/// private:  exists only to satify virtual class
rocksdb::ColumnFamilyHandle * RocksDBWrapper::DefaultColumnFamily() const {
    READ_LOCKER(lock, _rwlock);
    return RocksDBColumnFamily::invalid()->unwrapCF(); // contains _db->DefaultColumnFamily already

} // RocksDBWrapper::DefaultColumnFamily




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


/// @brief rocksdb populates handlesPtr with its vector of handles, swap them out
///   for wrapped handles
  void RocksDBWrapper::buildCFWrappers(std::vector<rocksdb::ColumnFamilyHandle *> & newHandles) {
  size_t index;

  for (index=0; index < newHandles.size(); ++index) {
    rocksdb::ColumnFamilyHandle * rHandle;
    RocksDBWrapperCFHandle * wrap;

    rHandle = newHandles[index];
    wrap = new RocksDBWrapperCFHandle(this, rHandle);
    _cfWrappers.push_back(std::shared_ptr<RocksDBWrapperCFHandle>(wrap));
    _handlesPtr->push_back(wrap);
  } // for

  RocksDBColumnFamily::_invalid = new RocksDBWrapperCFHandle(this, _db->DefaultColumnFamily());
  _cfWrappers.push_back(std::shared_ptr<RocksDBWrapperCFHandle>(RocksDBColumnFamily::_invalid));

}  // RocksDBWrapper::buildCFWrappers


/// @brief rocksdb populates handlesPtr with its vector of handles, swap them out
///   for wrapped handles
  void RocksDBWrapper::updateCFWrappers(std::vector<rocksdb::ColumnFamilyHandle *> & newHandles) {
  size_t index;
  TRI_ASSERT(nullptr != _handlesPtr);
  TRI_ASSERT(_handlesPtr->size() == newHandles.size());

  for (index=0; index < _handlesPtr->size(); ++index) {
    RocksDBWrapperCFHandle * wrap;

    wrap = (RocksDBWrapperCFHandle *)_handlesPtr->at(index);
    wrap->SetColumnFamilyHandle(newHandles[index]);
  } // for

  RocksDBColumnFamily::_invalid->SetColumnFamilyHandle(_db->DefaultColumnFamily());

}  // RocksDBWrapper::updateCFWrappers



rocksdb::ColumnFamilyHandle * RocksDBWrapper::unwrapCF(rocksdb::ColumnFamilyHandle * wrapper) {

  return ((RocksDBWrapperCFHandle *)wrapper)->GetColumnFamilyHandle();
}

rocksdb::ColumnFamilyHandle * RocksDBWrapper::unwrapCF(const rocksdb::ColumnFamilyHandle * wrapper) {

  return ((RocksDBWrapperCFHandle *)wrapper)->GetColumnFamilyHandle();
}


  // Returns the name of the column family associated with the current handle.
  const std::string& RocksDBWrapperCFHandle::GetName() const {
    READ_LOCKER(lock, _db->rwlock());
    return _cfHandle->GetName();
  }

  // Returns the ID of the column family associated with the current handle.
  uint32_t RocksDBWrapperCFHandle::GetID() const {
    READ_LOCKER(lock, _db->rwlock());
    return _cfHandle->GetID();
  }

  // Fills "*desc" with the up-to-date descriptor of the column family
  // associated with this handle. Since it fills "*desc" with the up-to-date
  // information, this call might internally lock and release DB mutex to
  // access the up-to-date CF options.  In addition, all the pointer-typed
  // options cannot be referenced any longer than the original options exist.
  //
  // Note that this function is not supported in RocksDBLite.
  rocksdb::Status RocksDBWrapperCFHandle::GetDescriptor(
    rocksdb::ColumnFamilyDescriptor* desc) {
    READ_LOCKER(lock, _db->rwlock());
    return _cfHandle->GetDescriptor(desc);
  }

  // Returns the comparator of the column family associated with the
  // current handle.
  const rocksdb::Comparator* RocksDBWrapperCFHandle::GetComparator() const {
    READ_LOCKER(lock, _db->rwlock());
    return _cfHandle->GetComparator();
  }



}  // namespace arangodb
