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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_DATA_PROTECTOR_H
#define ARANGODB_BASICS_DATA_PROTECTOR_H 1

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

#include "Basics/Common.h"

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief DataProtector, a template class to manage a single atomic
/// value (can be a pointer to some class), optimized for many fast
/// readers and slow writers using lockfree technology.
/// The template parameter Nr should be on the order of magnitude of the
/// maximal number of concurrently running threads.
/// Usage:
///   Put an instance of the DataProtector next to the atomic value you
///   want to protect, as in:
///     atomic<SomeClass*> p;
///     DataProtector<64> prot;
///   If you want to read p and *p, do
///     auto unuser(prot.use());
///     auto pSeen = p;
///   in the scope where you want to read p and *p and then only use pSeen
///   and *pSeen.
///   It is automatically released when the unuser instances goes out of scope.
///   This is guaranteed to be very fast, even when multiple threads do it
///   concurrently.
///   If you want to change p (and call delete on the old value, say), then
///     auto oldp = p;       // save the old value of p
///     p = <new Value>;     // just assign p whenever you see fit
///     prot.scan();         // This will block until no thread is reading
///                          // the old value any more.
///     delete oldp;         // guaranteed to be safe
///   This can be a slow operation and only one thread should perform it
///   at a time. Use a mutex to ensure this.
///   Please note:
///     - The value of p *can* change under the feet of the reading threads,
///       which is why you need to use the pSeen variable. However, you know
///       that as long as unused is in scope, pSeen remains valid.
///     - The DataProtector instances needs 64*Nr bytes of memory.
///     - DataProtector.cpp needs to contain an explicit template
///       instantiation for all values of Nr used in the executable.
////////////////////////////////////////////////////////////////////////////////

class DataProtector {
  static constexpr int DATA_PROTECTOR_MULTIPLICITY = 64;

  // disabled because of:
  //   note: uses ‘void* operator new(std::size_t)’, which does not have an
  //   alignment parameter note: use ‘-faligned-new’ to enable C++17
  //   over-aligned new support
  // struct alignas(64) Entry {  // 64 is the size of a cache line,
  struct Entry {  // 64 is the size of a cache line,
    // it is important that different list entries lie in different
    // cache lines.
    std::atomic<int> _count;

    Entry() : _count(0) {}
  };

  typename std::aligned_storage<sizeof(Entry), alignof(Entry)>::type _list[DATA_PROTECTOR_MULTIPLICITY];

  static std::atomic<int> _last;

  static thread_local int _mySlot;

 public:
  // A class to automatically unuse the DataProtector:
  class UnUser {
    DataProtector* _prot;
    int _id;

   public:
    UnUser(DataProtector* p, int i) : _prot(p), _id(i) {}

    ~UnUser() {
      if (_prot != nullptr) {
        _prot->unUse(_id);
      }
    }

    // A move constructor
    UnUser(UnUser&& that) : _prot(that._prot), _id(that._id) {
      // Note that return value optimization will usually avoid
      // this move constructor completely. However, it has to be
      // present for the program to compile.
      that._prot = nullptr;
    }

    // Explicitly delete the others:
    UnUser(UnUser const& that) = delete;
    UnUser& operator=(UnUser const& that) = delete;
    UnUser& operator=(UnUser&& that) = delete;
    UnUser() = delete;
  };

  DataProtector() {
    // initialize uninitialized memory
    for (int i = 0; i < DATA_PROTECTOR_MULTIPLICITY; i++) {
      new (_list + i) Entry;
    }
  }

  ~DataProtector() {
    for (int i = 0; i < DATA_PROTECTOR_MULTIPLICITY; i++) {
      reinterpret_cast<Entry*>(_list + i)->~Entry();
    }
  }

  UnUser use() {
    int id = getMyId();
    // this is implicitly using memory_order_seq_cst
    reinterpret_cast<Entry*>(_list + id)->_count++;
    return UnUser(this, id);  // return value optimization!
  }

  void scan() {
    for (int i = 0; i < DATA_PROTECTOR_MULTIPLICITY; i++) {
      while (reinterpret_cast<Entry*>(_list + i)->_count > 0) {
        // let other threads do some work while we're waiting
        std::this_thread::sleep_for(std::chrono::microseconds(250));
      }
    }
  }

 private:
  void unUse(int id) {
    // this is implicitly using memory_order_seq_cst
    reinterpret_cast<Entry*>(_list + id)->_count--;
  }

  int getMyId();
};

}  // namespace basics
}  // namespace arangodb

#endif
