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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_DEADLOCK_DETECTOR_H
#define ARANGODB_BASICS_DEADLOCK_DETECTOR_H 1

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"

namespace arangodb {
namespace basics {

template <typename TID, typename T>
class DeadlockDetector {
 public:
  explicit DeadlockDetector(bool enabled) : _enabled(enabled) {}
  ~DeadlockDetector() = default;

  DeadlockDetector(DeadlockDetector const&) = delete;
  DeadlockDetector& operator=(DeadlockDetector const&) = delete;

 public:
  /// @brief add a thread to the list of blocked threads
  int detectDeadlock(TID tid, T const* value, bool isWrite) {
    MUTEX_LOCKER(mutexLocker, _lock);
    return detectDeadlockNoLock(tid, value, isWrite);
  }

  /// @brief add a reader to the list of blocked readers
  int setReaderBlocked(TID tid, T const* value) {
    return setBlocked(tid, value, false);
  }

  /// @brief add a writer to the list of blocked writers
  int setWriterBlocked(TID tid, T const* value) {
    return setBlocked(tid, value, true);
  }

  /// @brief remove a reader from the list of blocked readers
  void unsetReaderBlocked(TID tid, T const* value) {
    unsetBlocked(tid, value, false);
  }

  /// @brief remove a writer from the list of blocked writers
  void unsetWriterBlocked(TID tid, T const* value) {
    unsetBlocked(tid, value, true);
  }

  /// @brief add a reader to the list of active readers
  void addReader(TID tid, T const* value, bool wasBlockedBefore) {
    addActive(tid, value, false, wasBlockedBefore);
  }

  /// @brief add a writer to the list of active writers
  void addWriter(TID tid, T const* value, bool wasBlockedBefore) {
    addActive(tid, value, true, wasBlockedBefore);
  }

  /// @brief unregister a reader from the list of active readers
  void unsetReader(TID tid, T const* value) { unsetActive(tid, value, false); }

  /// @brief unregister a writer from the list of active writers
  void unsetWriter(TID tid, T const* value) { unsetActive(tid, value, true); }

  /// @brief enable / disable
  void enabled(bool value) {
    MUTEX_LOCKER(mutexLocker, _lock);
    _enabled = value;
  }

  /// @brief return the enabled status
  bool enabled() {
    MUTEX_LOCKER(mutexLocker, _lock);
    return _enabled;
  }

 private:
  /// @brief add a thread to the list of blocked threads
  int detectDeadlockNoLock(TID tid, T const* value, bool isWrite) const {
    if (!_enabled) {
      return TRI_ERROR_NO_ERROR;
    }

    struct StackValue {
      StackValue(TID tid, T const* value, bool isWrite)
          : tid(tid), value(value), isWrite(isWrite) {}
      TID tid;
      T const* value;
      bool isWrite;
    };

    std::unordered_set<TID> visited;
    std::vector<StackValue> stack{StackValue(tid, value, isWrite)};

    while (!stack.empty()) {
      StackValue top = stack.back();  // intentionally copy StackValue
      stack.pop_back();

      auto it = _active.find(top.value);

      if (it != _active.end()) {
        if (!top.isWrite) {
          // we are a reader
          bool other = (*it).second.second;

          if (other) {
            // other is a writer
            TID otherTid = *((*it).second.first.begin());

            if (visited.find(otherTid) != visited.end()) {
              return TRI_ERROR_DEADLOCK;
            }

            auto it2 = _blocked.find(otherTid);

            if (it2 != _blocked.end()) {
              // writer thread is blocking...
              stack.emplace_back(otherTid, (*it2).second.first, other);
            }
          }
        } else {
          // we are a writer

          // other is either a reader or a writer
          for (auto const& otherTid : (*it).second.first) {
            if (visited.find(otherTid) != visited.end()) {
              return TRI_ERROR_DEADLOCK;
            }

            auto it2 = _blocked.find(otherTid);

            if (it2 != _blocked.end()) {
              // writer thread is blocking...
              stack.emplace_back(otherTid, (*it2).second.first, (*it).second.second);
            }
          }
        }
      }

      visited.emplace(top.tid);
    }

    // no deadlock
    return TRI_ERROR_NO_ERROR;
  }

  /// @brief add a thread to the list of blocked threads
  int setBlocked(TID tid, T const* value, bool isWrite) {
    MUTEX_LOCKER(mutexLocker, _lock);

    if (!_enabled) {
      return TRI_ERROR_NO_ERROR;
    }

    bool const inserted = _blocked.emplace(tid, std::make_pair(value, isWrite)).second;

    if (!inserted) {
      // we're already blocking. should never happen
      return TRI_ERROR_DEADLOCK;
    }

    try {
      int res = detectDeadlockNoLock(tid, value, isWrite);

      if (res != TRI_ERROR_NO_ERROR) {
        // clean up
        auto erased = _blocked.erase(tid);
        TRI_ASSERT(erased == 1);
      }

      return res;
    } catch (...) {
      // clean up
      auto erased = _blocked.erase(tid);
      TRI_ASSERT(erased == 1);
      throw;
    }
  }

  /// @brief remove a thread from the list of blocked threads
  void unsetBlocked(TID tid, T const* value, bool isWrite) {
    MUTEX_LOCKER(mutexLocker, _lock);

    if (!_enabled) {
      return;
    }

    auto erased = _blocked.erase(tid);
    TRI_ASSERT(erased == 1);
  }

  /// @brief unregister a thread from the list of active threads
  void unsetActive(TID tid, T const* value, bool isWrite) {
    MUTEX_LOCKER(mutexLocker,
                 _lock);  // note: this lock is expensive when many threads compete

    if (!_enabled) {
      return;
    }

    auto it = _active.find(value);

    if (it == _active.end()) {
      // should not happen, but definitely nothing to do here
      return;
    }

    bool wasLast;

    if (isWrite) {
      // the thread should have held the resource in write mode
      TRI_ASSERT((*it).second.second);
      // nobody else should have registered for the same resource
      TRI_ASSERT((*it).second.first.size() == 1);
      // a writer is exclusive, so we're always the last one
      wasLast = true;
    } else {
      // we shouldn't have another writer
      TRI_ASSERT(!(*it).second.second);
      // there should be at least one (reader) entry (this can be ourselves)
      TRI_ASSERT((*it).second.first.size() >= 1);

      // we ourselves should be present in the threads list
      TRI_ASSERT((*it).second.first.find(tid) != (*it).second.first.end());
      // if there's only only thread registered, it must be us
      wasLast = ((*it).second.first.size() == 1);

      if (!wasLast) {
        // we're not the last thread, so we simply unregister ourselves from the
        // list
        auto erased = (*it).second.first.erase(tid);
        TRI_ASSERT(erased == 1);
        // we shouldn't be in the list anymore
        TRI_ASSERT((*it).second.first.find(tid) == (*it).second.first.end());
      }
      // still the write bit shouldn't be set
      TRI_ASSERT(!(*it).second.second);
      // and there should be at least one entry now:
      // if we were last, it will remain there and the cleanup will happen below
      // if we were not last, we have removed ourselves from the list, but there
      // must be at least one other thread in the list - otherwise we would have
      // been last!
      TRI_ASSERT((*it).second.first.size() >= 1);
    }

    if (wasLast) {
      // delete last reader/writer
      auto erased = _active.erase(value);
      TRI_ASSERT(erased == 1);
    }
  }

  /// @brief add a reader/writer to the list of active threads
  void addActive(TID tid, T const* value, bool isWrite, bool wasBlockedBefore) {
    MUTEX_LOCKER(mutexLocker,
                 _lock);  // note: this lock is expensive when many threads compete

    if (!_enabled) {
      return;
    }

    auto it = _active.find(value);

    if (it == _active.end()) {
      // no one else there. simply register us
      _active.emplace(value, std::make_pair(std::unordered_set<TID>({tid}), isWrite));
    } else {
      // someone else is already there
      // we're expecting one or many readers
      TRI_ASSERT(!(*it).second.first.empty());
      TRI_ASSERT(!(*it).second.second);
      // if someone else is already there, this must be a reader, as readers can
      // share a resource, but writers are exclusive
      TRI_ASSERT(!isWrite);
      auto result = (*it).second.first.emplace(tid);
      TRI_ASSERT(result.second);
      TRI_ASSERT(!(*it).second.second);
      TRI_ASSERT((*it).second.first.find(tid) != (*it).second.first.end());
    }

    if (wasBlockedBefore) {
      auto erased = _blocked.erase(tid);
      TRI_ASSERT(erased == 1);
    }
  }

 private:
  /// @brief lock for managing the data structures
  arangodb::Mutex _lock;

  /// @brief threads currently blocked
  std::unordered_map<TID, std::pair<T const*, bool>> _blocked;

  /// @brief threads currently holding locks
  std::unordered_map<T const*, std::pair<std::unordered_set<TID>, bool>> _active;

  /// @brief whether or not the detector is enabled
  bool _enabled;
};

}  // namespace basics
}  // namespace arangodb

#endif
