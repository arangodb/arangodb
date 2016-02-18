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

#ifndef LIB_BASICS_DEADLOCK_DETECTOR_H
#define LIB_BASICS_DEADLOCK_DETECTOR_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/threads.h"

namespace arangodb {
namespace basics {

template <typename T>
class DeadlockDetector {
 public:
  explicit DeadlockDetector(bool enabled) : _enabled(enabled) {};
  ~DeadlockDetector() = default;

  DeadlockDetector(DeadlockDetector const&) = delete;
  DeadlockDetector& operator=(DeadlockDetector const&) = delete;

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a thread to the list of blocked threads
  ////////////////////////////////////////////////////////////////////////////////

  int detectDeadlock(T const* value, bool isWrite) {
    auto tid = TRI_CurrentThreadId();

    MUTEX_LOCKER(mutexLocker, _lock);
    return detectDeadlock(value, tid, isWrite);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a reader to the list of blocked readers
  ////////////////////////////////////////////////////////////////////////////////

  int setReaderBlocked(T const* value) { return setBlocked(value, false); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a writer to the list of blocked writers
  ////////////////////////////////////////////////////////////////////////////////

  int setWriterBlocked(T const* value) { return setBlocked(value, true); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove a reader from the list of blocked readers
  ////////////////////////////////////////////////////////////////////////////////

  void unsetReaderBlocked(T const* value) { unsetBlocked(value, false); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove a writer from the list of blocked writers
  ////////////////////////////////////////////////////////////////////////////////

  void unsetWriterBlocked(T const* value) { unsetBlocked(value, true); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a reader to the list of active readers
  ////////////////////////////////////////////////////////////////////////////////

  void addReader(T const* value, bool wasBlockedBefore) {
    addActive(value, false, wasBlockedBefore);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a writer to the list of active writers
  ////////////////////////////////////////////////////////////////////////////////

  void addWriter(T const* value, bool wasBlockedBefore) {
    addActive(value, true, wasBlockedBefore);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief unregister a reader from the list of active readers
  ////////////////////////////////////////////////////////////////////////////////

  void unsetReader(T const* value) { unsetActive(value, false); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief unregister a writer from the list of active writers
  ////////////////////////////////////////////////////////////////////////////////

  void unsetWriter(T const* value) { unsetActive(value, true); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief enable / disable
  ////////////////////////////////////////////////////////////////////////////////

  void enabled(bool value) {
    MUTEX_LOCKER(mutexLocker, _lock);
    _enabled = value;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief return the enabled status
  ////////////////////////////////////////////////////////////////////////////////

  bool enabled() {
    MUTEX_LOCKER(mutexLocker, _lock);
    return _enabled;
  }

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a thread to the list of blocked threads
  ////////////////////////////////////////////////////////////////////////////////

  int detectDeadlock(T const* value, TRI_tid_t tid, bool isWrite) const {
    if (!_enabled) {
      return TRI_ERROR_NO_ERROR;
    }

    struct StackValue {
      StackValue(T const* value, TRI_tid_t tid, bool isWrite)
          : value(value), tid(tid), isWrite(isWrite) {}
      T const* value;
      TRI_tid_t tid;
      bool isWrite;
    };

    std::unordered_set<TRI_tid_t> visited;
    std::vector<StackValue> stack;
    stack.emplace_back(StackValue(value, tid, isWrite));

    while (!stack.empty()) {
      StackValue top = stack.back();  // intentionally copy StackValue
      stack.pop_back();

      if (!top.isWrite) {
        // we are a reader
        auto it = _active.find(top.value);

        if (it != _active.end()) {
          bool other = (*it).second.second;

          if (other) {
            // other is a writer
            TRI_tid_t otherTid = *((*it).second.first.begin());

            if (visited.find(otherTid) != visited.end()) {
              return TRI_ERROR_DEADLOCK;
            }

            auto it2 = _blocked.find(otherTid);

            if (it2 != _blocked.end()) {
              // writer thread is blocking...
              stack.emplace_back(
                  StackValue((*it2).second.first, otherTid, other));
            }
          }
        }
      } else {
        // we are a writer
        auto it = _active.find(top.value);

        if (it != _active.end()) {
          // other is either a reader or a writer
          for (auto const& otherTid : (*it).second.first) {
            if (visited.find(otherTid) != visited.end()) {
              return TRI_ERROR_DEADLOCK;
            }

            auto it2 = _blocked.find(otherTid);

            if (it2 != _blocked.end()) {
              // writer thread is blocking...
              stack.emplace_back(StackValue((*it2).second.first, otherTid,
                                            (*it).second.second));
            }
          }
        }
      }

      visited.emplace(top.tid);
    }

    // no deadlock
    return TRI_ERROR_NO_ERROR;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a thread to the list of blocked threads
  ////////////////////////////////////////////////////////////////////////////////

  int setBlocked(T const* value, bool isWrite) {
    auto tid = TRI_CurrentThreadId();

    MUTEX_LOCKER(mutexLocker, _lock);

    if (!_enabled) {
      return TRI_ERROR_NO_ERROR;
    }

    auto it = _blocked.find(tid);

    if (it != _blocked.end()) {
      // we're already blocking. should never happend
      return TRI_ERROR_DEADLOCK;
    }

    _blocked.emplace(tid, std::make_pair(value, isWrite));

    try {
      int res = detectDeadlock(value, tid, isWrite);

      if (res != TRI_ERROR_NO_ERROR) {
        // clean up
        _blocked.erase(tid);
      }

      return res;
    } catch (...) {
      // clean up
      _blocked.erase(tid);
      throw;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove a thread from the list of blocked threads
  ////////////////////////////////////////////////////////////////////////////////

  void unsetBlocked(T const* value, bool isWrite) {
    auto tid = TRI_CurrentThreadId();

    MUTEX_LOCKER(mutexLocker, _lock);

    if (!_enabled) {
      return;
    }

    _blocked.erase(tid);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief unregister a thread from the list of active threads
  ////////////////////////////////////////////////////////////////////////////////

  void unsetActive(T const* value, bool isWrite) {
    auto tid = TRI_CurrentThreadId();

    MUTEX_LOCKER(mutexLocker, _lock);

    if (!_enabled) {
      return;
    }

    auto it = _active.find(value);

    if (it == _active.end()) {
      // should not happen, but definitely nothing to do here
      return;
    }

    if (isWrite) {
      TRI_ASSERT((*it).second.second);
      TRI_ASSERT((*it).second.first.size() == 1);
      // remove whole entry
      _active.erase(value);
    } else {
      TRI_ASSERT(!(*it).second.second);
      TRI_ASSERT((*it).second.first.size() >= 1);

      (*it).second.first.erase(tid);
      if ((*it).second.first.empty()) {
        // remove last reader
        _active.erase(value);
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add a reader/writer to the list of active threads
  ////////////////////////////////////////////////////////////////////////////////

  void addActive(T const* value, bool isWrite, bool wasBlockedBefore) {
    auto tid = TRI_CurrentThreadId();

    MUTEX_LOCKER(mutexLocker, _lock);

    if (!_enabled) {
      return;
    }

    auto it = _active.find(value);

    if (it == _active.end()) {
      _active.emplace(
          value, std::make_pair(std::unordered_set<TRI_tid_t>({tid}), isWrite));
    } else {
      TRI_ASSERT(!(*it).second.first.empty());
      TRI_ASSERT(!(*it).second.second);
      TRI_ASSERT(!isWrite);
#ifdef TRI_ENABLE_MAINTAINER_MODE
      auto result =
#endif
        (*it).second.first.emplace(tid);
#ifdef TRI_ENABLE_MAINTAINER_MODE
      TRI_ASSERT(result.second);
#endif
    }

    if (wasBlockedBefore) {
      _blocked.erase(tid);
    }
  }

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lock for managing the data structures
  ////////////////////////////////////////////////////////////////////////////////

  arangodb::Mutex _lock;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief threads currently blocked
  ////////////////////////////////////////////////////////////////////////////////

  std::unordered_map<TRI_tid_t, std::pair<T const*, bool>> _blocked;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief threads currently holding locks
  ////////////////////////////////////////////////////////////////////////////////

  std::unordered_map<T const*, std::pair<std::unordered_set<TRI_tid_t>, bool>>
      _active;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the detector is enabled
  ////////////////////////////////////////////////////////////////////////////////

  bool _enabled;
};

}  // namespace arangodb::basics
}  // namespace arangodb

#endif
