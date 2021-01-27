////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

#include "gtest/gtest.h"

#include <chrono>

using namespace arangodb::basics;

TEST(ReadWriteLockTest, testTryLockWrite) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try lock write
  ASSERT_TRUE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try write-locking again
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try write-locking again, with timeout
  ASSERT_FALSE(lock.lockWrite(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try read-locking 
  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(ReadWriteLockTest, testLockWrite) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock write
  lock.lockWrite();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try write-locking again
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try write-locking again, with timeout
  ASSERT_FALSE(lock.lockWrite(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try read-locking 
  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(ReadWriteLockTest, testTryLockRead) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try lock read
  ASSERT_TRUE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try read-locking again
  ASSERT_TRUE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // read-lock again
  lock.lockRead();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking 
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking again, with timeout
  ASSERT_FALSE(lock.lockWrite(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // unlock one level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());
  
  // unlock one another level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());
  
  // unlock final level
  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_TRUE(lock.tryLockWrite());
}

TEST(ReadWriteLockTest, testLockRead) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock read
  lock.lockRead();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try read-locking again
  ASSERT_TRUE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // read-lock again
  lock.lockRead();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking 
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking again, with timeout
  ASSERT_FALSE(lock.lockWrite(std::chrono::microseconds(1000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // unlock one level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());
  
  // unlock one another level
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_FALSE(lock.tryLockWrite());
  
  // unlock final level
  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  ASSERT_TRUE(lock.tryLockWrite());
}

TEST(ReadWriteLockTest, testLockWriteAttempted) {
  ReadWriteLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock write
  ASSERT_TRUE(lock.lockWrite(std::chrono::microseconds(1000000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try locking again
  ASSERT_FALSE(lock.lockWrite(std::chrono::microseconds(1000000)));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
}
