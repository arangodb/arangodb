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

#ifndef ARANGOD_UTILS_COLLECTION_WRITE_LOCKER_H
#define ARANGOD_UTILS_COLLECTION_WRITE_LOCKER_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/transaction.h"

namespace arangodb {

class CollectionWriteLocker {
 public:
  CollectionWriteLocker(CollectionWriteLocker const&) = delete;
  CollectionWriteLocker& operator=(CollectionWriteLocker const&) = delete;

  /// @brief create the locker
  CollectionWriteLocker(arangodb::LogicalCollection* collection, bool useDeadlockDetector, bool doLock)
      : _collection(collection), _useDeadlockDetector(useDeadlockDetector), _doLock(false) {
    if (doLock) {
      int res = _collection->beginWriteTimed(_useDeadlockDetector, 0, TRI_TRANSACTION_DEFAULT_SLEEP_DURATION);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      _doLock = true;
    }
  }

  /// @brief destroy the locker
  ~CollectionWriteLocker() { unlock(); }

  /// @brief release the lock
  inline void unlock() {
    if (_doLock) {
      _collection->endWrite(_useDeadlockDetector);
      _doLock = false;
    }
  }

 private:
  /// @brief collection pointer
  arangodb::LogicalCollection* _collection;

  /// @brief whether or not to use the deadlock detector
  bool const _useDeadlockDetector;

  /// @brief lock flag
  bool _doLock;
};
}

#endif
