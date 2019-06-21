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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////


#include "RocksDBMetaCollection.h"

#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Utils/OperationOptions.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

RocksDBMetaCollection::RocksDBMetaCollection(LogicalCollection& collection,
                                             VPackSlice const& info)
: PhysicalCollection(collection, info),
  _objectId(basics::VelocyPackHelper::stringUInt64(info, "objectId")) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  VPackSlice s = info.get("isVolatile");
  if (s.isBoolean() && s.getBoolean()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
                                   TRI_ERROR_BAD_PARAMETER,
                                   "volatile collections are unsupported in the RocksDB engine");
  }
  
  TRI_ASSERT(_logicalCollection.isAStub() || _objectId != 0);
  rocksutils::globalRocksEngine()->addCollectionMapping(_objectId, _logicalCollection.vocbase().id(),
                                                        _logicalCollection.id());
}

RocksDBMetaCollection::RocksDBMetaCollection(LogicalCollection& collection,
                                             PhysicalCollection const* physical)
: PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
  _objectId(static_cast<RocksDBMetaCollection const*>(physical)->_objectId) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  rocksutils::globalRocksEngine()->addCollectionMapping(_objectId, _logicalCollection.vocbase().id(), _logicalCollection.id());
}

std::string const& RocksDBMetaCollection::path() const {
  return StaticStrings::Empty;  // we do not have any path
}

/// @brief write locks a collection, with a timeout
int RocksDBMetaCollection::lockWrite(double timeout) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;
  
  while (true) {
    TRY_WRITE_LOCKER(locker, _exclusiveLock);
    
    if (locker.isLocked()) {
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }
    
    double now = TRI_microtime();
    
    if (waitTime == 0) {  // initialize times
      // set end time for lock waiting
      if (timeout <= 0.0) {
        timeout = defaultLockTimeout;
      }
      
      startTime = now;
      waitTime = 1;
    }
    
    if (now > startTime + timeout) {
      LOG_TOPIC("d1e53", TRACE, arangodb::Logger::ENGINES)
      << "timed out after " << timeout << " s waiting for write-lock on collection '"
      << _logicalCollection.name() << "'";
      
      return TRI_ERROR_LOCK_TIMEOUT;
    }
    
    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief write unlocks a collection
void RocksDBMetaCollection::unlockWrite() { _exclusiveLock.unlockWrite(); }

/// @brief read locks a collection, with a timeout
int RocksDBMetaCollection::lockRead(double timeout) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;
  
  while (true) {
    TRY_READ_LOCKER(locker, _exclusiveLock);
    
    if (locker.isLocked()) {
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }
    
    double now = TRI_microtime();
    
    if (waitTime == 0) {  // initialize times
      // set end time for lock waiting
      if (timeout <= 0.0) {
        timeout = defaultLockTimeout;
      }
      
      startTime = now;
      waitTime = 1;
    }
    
    if (now > startTime + timeout) {
      LOG_TOPIC("dcbd2", TRACE, arangodb::Logger::ENGINES)
      << "timed out after " << timeout << " s waiting for read-lock on collection '"
      << _logicalCollection.name() << "'";
      
      return TRI_ERROR_LOCK_TIMEOUT;
    }
    
    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
      
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief read unlocks a collection
void RocksDBMetaCollection::unlockRead() { _exclusiveLock.unlockRead(); }


void RocksDBMetaCollection::trackWaitForSync(arangodb::transaction::Methods* trx,
                                             OperationOptions& options) {
  if (_logicalCollection.waitForSync() && !options.isRestore) {
    options.waitForSync = true;
  }
  
  if (options.waitForSync) {
    trx->state()->waitForSync(true);
  }
}
