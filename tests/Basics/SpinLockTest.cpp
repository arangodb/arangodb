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
#include "Basics/ReadWriteSpinLock.h"
#include "Basics/SpinLocker.h"
#include "Basics/SpinUnlocker.h"

#include "gtest/gtest.h"

using namespace arangodb::basics;

TEST(SpinLockTest, testSpinLocker) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // write
  { 
    SpinLocker guard(SpinLocker::Mode::Write, lock, true, SpinLocker::Effort::Succeed);
    ASSERT_TRUE(lock.isLocked());
    ASSERT_FALSE(lock.isLockedRead());
    ASSERT_TRUE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
  }
  ASSERT_FALSE(lock.isLocked());
  
  { 
    SpinLocker guard(SpinLocker::Mode::Write, lock, false, SpinLocker::Effort::Succeed);
    ASSERT_FALSE(lock.isLocked());
    ASSERT_FALSE(lock.isLockedRead());
    ASSERT_FALSE(lock.isLockedWrite());
    ASSERT_FALSE(guard.isLocked());
  }
  ASSERT_FALSE(lock.isLocked());
  
  // read
  { 
    SpinLocker guard(SpinLocker::Mode::Read, lock, true, SpinLocker::Effort::Succeed);
    ASSERT_TRUE(lock.isLocked());
    ASSERT_TRUE(lock.isLockedRead());
    ASSERT_FALSE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
  }
  ASSERT_FALSE(lock.isLocked());
  
  { 
    SpinLocker guard(SpinLocker::Mode::Read, lock, false, SpinLocker::Effort::Succeed);
    ASSERT_FALSE(lock.isLocked());
    ASSERT_FALSE(lock.isLockedRead());
    ASSERT_FALSE(lock.isLockedWrite());
    ASSERT_FALSE(guard.isLocked());
  }
  ASSERT_FALSE(lock.isLocked());
}

TEST(SpinLockTest, testNestedSpinLocker) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // write
  { 
    SpinLocker guard(SpinLocker::Mode::Write, lock, true, SpinLocker::Effort::Succeed);
    ASSERT_TRUE(lock.isLocked());
    ASSERT_FALSE(lock.isLockedRead());
    ASSERT_TRUE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());

    {
      SpinLocker inner(SpinLocker::Mode::Write, lock, true, SpinLocker::Effort::Try);
      ASSERT_TRUE(lock.isLocked());
      ASSERT_FALSE(lock.isLockedRead());
      ASSERT_TRUE(lock.isLockedWrite());
      ASSERT_TRUE(guard.isLocked());
      ASSERT_FALSE(inner.isLocked());
    }
    ASSERT_TRUE(lock.isLocked());
    ASSERT_FALSE(lock.isLockedRead());
    ASSERT_TRUE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
    
    {
      SpinLocker inner(SpinLocker::Mode::Read, lock, true, SpinLocker::Effort::Try);
      ASSERT_TRUE(lock.isLocked());
      ASSERT_FALSE(lock.isLockedRead());
      ASSERT_TRUE(lock.isLockedWrite());
      ASSERT_TRUE(guard.isLocked());
      ASSERT_FALSE(inner.isLocked());
    }
    ASSERT_TRUE(lock.isLocked());
    ASSERT_FALSE(lock.isLockedRead());
    ASSERT_TRUE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
    
    {
      SpinUnlocker inner(SpinUnlocker::Mode::Write, lock);
      ASSERT_FALSE(lock.isLocked());
      ASSERT_FALSE(lock.isLockedRead());
      ASSERT_FALSE(lock.isLockedWrite());
      ASSERT_TRUE(guard.isLocked());
      ASSERT_FALSE(inner.isLocked());
    }
    ASSERT_TRUE(lock.isLocked());
    ASSERT_FALSE(lock.isLockedRead());
    ASSERT_TRUE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
  }
  ASSERT_FALSE(lock.isLocked());
  
  // read
  { 
    SpinLocker guard(SpinLocker::Mode::Read, lock, true, SpinLocker::Effort::Succeed);
    ASSERT_TRUE(lock.isLocked());
    ASSERT_TRUE(lock.isLockedRead());
    ASSERT_FALSE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
    
    {
      SpinLocker inner(SpinLocker::Mode::Write, lock, true, SpinLocker::Effort::Try);
      ASSERT_TRUE(lock.isLocked());
      ASSERT_TRUE(lock.isLockedRead());
      ASSERT_FALSE(lock.isLockedWrite());
      ASSERT_TRUE(guard.isLocked());
      ASSERT_FALSE(inner.isLocked());
    }
    ASSERT_TRUE(lock.isLocked());
    ASSERT_TRUE(lock.isLockedRead());
    ASSERT_FALSE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
    
    {
      SpinLocker inner(SpinLocker::Mode::Read, lock, true, SpinLocker::Effort::Try);
      ASSERT_TRUE(lock.isLocked());
      ASSERT_TRUE(lock.isLockedRead());
      ASSERT_FALSE(lock.isLockedWrite());
      ASSERT_TRUE(guard.isLocked());
      ASSERT_TRUE(inner.isLocked());
    }
    ASSERT_TRUE(lock.isLocked());
    ASSERT_TRUE(lock.isLockedRead());
    ASSERT_FALSE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
    
    {
      SpinUnlocker inner(SpinUnlocker::Mode::Read, lock);
      ASSERT_FALSE(lock.isLocked());
      ASSERT_FALSE(lock.isLockedRead());
      ASSERT_FALSE(lock.isLockedWrite());
      ASSERT_TRUE(guard.isLocked());
      ASSERT_FALSE(inner.isLocked());
    }
    ASSERT_TRUE(lock.isLocked());
    ASSERT_TRUE(lock.isLockedRead());
    ASSERT_FALSE(lock.isLockedWrite());
    ASSERT_TRUE(guard.isLocked());
  }
  ASSERT_FALSE(lock.isLocked());
}

TEST(SpinLockTest, testTryLockWrite) {
  ReadWriteSpinLock lock;

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
  ASSERT_FALSE(lock.lockWrite(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try read-locking 
  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try read-locking again, with timeout
  ASSERT_FALSE(lock.lockRead(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(SpinLockTest, testLockWrite) {
  ReadWriteSpinLock lock;

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
  ASSERT_FALSE(lock.lockWrite(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try read-locking 
  ASSERT_FALSE(lock.tryLockRead());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  // try read-locking again, with timeout
  ASSERT_FALSE(lock.lockRead(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(SpinLockTest, testTryLockRead) {
  ReadWriteSpinLock lock;

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
  
  // try read-locking again, with timeout
  ASSERT_TRUE(lock.lockRead(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking 
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking again, with timeout
  ASSERT_FALSE(lock.lockWrite(10));
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

TEST(SpinLockTest, testLockRead) {
  ReadWriteSpinLock lock;

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
  
  // try read-locking again, with timeout
  ASSERT_TRUE(lock.lockRead(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking 
  ASSERT_FALSE(lock.tryLockWrite());
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // try write-locking again, with timeout
  ASSERT_FALSE(lock.lockWrite(10));
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

TEST(SpinLockTest, testLockWriteAttemptsZero) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock write
  ASSERT_TRUE(lock.lockWrite(0));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(SpinLockTest, testLockWriteAttemptsOne) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock write
  ASSERT_TRUE(lock.lockWrite(1));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
}

TEST(SpinLockTest, testLockReadAttemptsZero) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock read
  ASSERT_TRUE(lock.lockRead(0));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
}

TEST(SpinLockTest, testLockReadAttemptsOne) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock read
  ASSERT_TRUE(lock.lockRead(1));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
}

TEST(SpinLockTest, testLockWriteAttempted) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock write
  ASSERT_TRUE(lock.lockWrite(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  // try locking again
  ASSERT_FALSE(lock.lockWrite(5));
  ASSERT_FALSE(lock.lockWrite(0));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());
  
  ASSERT_FALSE(lock.lockRead(5));
  ASSERT_FALSE(lock.lockRead(0));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_TRUE(lock.isLockedWrite());

  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
}

TEST(SpinLockTest, testLockReadAttempted) {
  ReadWriteSpinLock lock;

  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  // lock read
  ASSERT_TRUE(lock.lockRead(10));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  // try locking again
  ASSERT_FALSE(lock.lockWrite(5));
  ASSERT_FALSE(lock.lockWrite(0));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  ASSERT_TRUE(lock.lockRead(5));
  ASSERT_TRUE(lock.lockRead(0));
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());

  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  lock.unlock();
  ASSERT_TRUE(lock.isLocked());
  ASSERT_TRUE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
  
  lock.unlock();
  ASSERT_FALSE(lock.isLocked());
  ASSERT_FALSE(lock.isLockedRead());
  ASSERT_FALSE(lock.isLockedWrite());
}
