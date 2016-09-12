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

#ifndef ARANGOD_VOCBASE_COMPACTION_LOCKER_H
#define ARANGOD_VOCBASE_COMPACTION_LOCKER_H 1

#include "Basics/Common.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

class CompactionPreventer {
 public:
  explicit CompactionPreventer(LogicalCollection* collection) 
      : _collection(collection) {
    _collection->preventCompaction();
  }
  
  ~CompactionPreventer() { _collection->allowCompaction(); }

 private:
  LogicalCollection* _collection;
};

class TryCompactionPreventer {
 public:
  explicit TryCompactionPreventer(LogicalCollection* collection) 
      : _collection(collection), _isLocked(false) {
    _isLocked = _collection->tryPreventCompaction();
  }
  
  ~TryCompactionPreventer() { 
    if (_isLocked) {
      _collection->allowCompaction(); 
    }
  }

  bool isLocked() const { return _isLocked; }

 private:
  LogicalCollection* _collection;
  bool _isLocked;
};

class CompactionLocker {
 public:
  explicit CompactionLocker(LogicalCollection* collection) 
      : _collection(collection) {
    _collection->lockForCompaction();
  }
  
  ~CompactionLocker() { 
    _collection->finishCompaction();
  }

 private:
  LogicalCollection* _collection;
};

class TryCompactionLocker {
 public:
  explicit TryCompactionLocker(LogicalCollection* collection) 
      : _collection(collection), _isLocked(false) {
    _isLocked = _collection->tryLockForCompaction();
  }
  
  ~TryCompactionLocker() { 
    if (_isLocked) {
      _collection->finishCompaction();
    }
  }

  bool isLocked() const { return _isLocked; }

 private:
  LogicalCollection* _collection;
  bool _isLocked;
};

}  // namespace arangodb

#endif
