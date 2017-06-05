// lock.h
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: riley@google.com (Michael Riley)
//
// \file
// Google-compatibility locking declarations and inline definitions
//
// Classes and functions here are no-ops (by design); proper locking requires
// actual implementation.

#ifndef FST_LIB_LOCK_H__
#define FST_LIB_LOCK_H__

#include <fst/compat.h>  // for DISALLOW_COPY_AND_ASSIGN

namespace fst {

using namespace std;

//
// Single initialization  - single-thread implementation
//

typedef int FstOnceType;

static const int FST_ONCE_INIT = 1;

inline int FstOnceInit(FstOnceType *once, void (*init)(void)) {
  if (*once)
    (*init)();
  *once = 0;
  return 0;
}

//
// Thread locking - single-thread (non-)implementation
//

class Mutex {
 public:
  Mutex() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class MutexLock {
 public:
  MutexLock(Mutex *) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MutexLock);
};

class ReaderMutexLock {
 public:
  ReaderMutexLock(Mutex *) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ReaderMutexLock);
};

// Reference counting - single-thread implementation
class RefCounter {
 public:
  RefCounter() : count_(1) {}

  int count() const { return count_; }
  int Incr() const { return ++count_; }
  int Decr() const {  return --count_; }

 private:
  mutable int count_;

  DISALLOW_COPY_AND_ASSIGN(RefCounter);
};

}  // namespace fst

#endif  // FST_LIB_LOCK_H__
