////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#include "Basics/UpgradeMutex.h"
#include "Basics/debugging.h"

/// Based on boost/1.78.0/boost/thread/v2/shared_mutex.hpp
/// Copyright Howard Hinnant 2007-2010.
/// Copyright Vicente J. Botet Escriba 2012.
///
///  Distributed under the Boost Software License, Version 1.0. (See
///  accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)

namespace arangodb::basics {

UpgradeMutex::UpgradeMutex() : _state{0} {}

UpgradeMutex::~UpgradeMutex() { [[maybe_unused]] std::lock_guard lock{_m}; }

// Exclusive ownership
void UpgradeMutex::lock() {
  std::unique_lock lock{_m};
  _gate1.wait(lock, [this] { return no_writer_no_upgrader(); });
  _state |= kWriteEntered;
  _gate2.wait(lock, [this] { return readers() == 0; });
}
bool UpgradeMutex::try_lock() {
  std::unique_lock lock{_m};
  if (_state != 0) {
    return false;
  }
  _state = kWriteEntered;
  return true;
}
void UpgradeMutex::unlock() {
  std::lock_guard _{_m};
  TRI_ASSERT(writer());
  TRI_ASSERT(!upgrader());
  TRI_ASSERT(readers() == 0);
  _state = 0;
  // notify all since multiple *lock_shared*() calls and a *lock_upgrade*()
  // call may be able to proceed in response to this notification
  _gate1.notify_all();
}

// Shared ownership
void UpgradeMutex::lock_shared() {
  std::unique_lock lock{_m};
  _gate1.wait(lock, [this] { return no_writer_no_max_readers(); });
  size_t num_readers = (_state & kReaders) + 1;
  _state &= ~kReaders;
  _state |= num_readers;
}
bool UpgradeMutex::try_lock_shared() {
  std::unique_lock lock{_m};
  if (!no_writer_no_max_readers()) {
    return false;
  }
  size_t num_readers = (_state & kReaders) + 1;
  _state &= ~kReaders;
  _state |= num_readers;
  return true;
}
void UpgradeMutex::unlock_shared() {
  std::lock_guard _{_m};
  TRI_ASSERT(readers() > 0);
  size_t num_readers = (_state & kReaders) - 1;
  _state &= ~kReaders;
  _state |= num_readers;
  if (!writer()) {
    if (num_readers == kReaders - 1) {
      _gate1.notify_one();
    }
  } else {
    if (num_readers == 0) {
      _gate2.notify_one();
    }
  }
}

// Upgrade ownership
void UpgradeMutex::lock_upgrade() {
  std::unique_lock lock{_m};
  _gate1.wait(lock, [this] { return no_writer_no_upgrader_no_max_readers(); });
  size_t num_readers = (_state & kReaders) + 1;
  _state &= ~kReaders;
  _state |= kUpgradableEntered | num_readers;
}
bool UpgradeMutex::try_lock_upgrade() {
  std::unique_lock lock{_m};
  if (!no_writer_no_upgrader_no_max_readers()) {
    return false;
  }
  size_t num_readers = (_state & kReaders) + 1;
  _state &= ~kReaders;
  _state |= kUpgradableEntered | num_readers;
  return true;
}
void UpgradeMutex::unlock_upgrade() {
  std::lock_guard _{_m};
  TRI_ASSERT(!writer());
  TRI_ASSERT(upgrader());
  TRI_ASSERT(readers() > 0);
  size_t num_readers = (_state & kReaders) - 1;
  _state &= ~(kUpgradableEntered | kReaders);
  _state |= num_readers;
  // notify all since both a *lock*() and a *lock_shared*() call
  // may be able to proceed in response to this notification
  _gate1.notify_all();
}

// Exclusive => Shared
void UpgradeMutex::unlock_and_lock_shared() {
  std::lock_guard _{_m};
  TRI_ASSERT(writer());
  TRI_ASSERT(!upgrader());
  TRI_ASSERT(readers() == 0);
  _state = 1;
  // notify all since multiple *lock_shared*() calls and a *lock_upgrade*()
  // call may be able to proceed in response to this notification
  _gate1.notify_all();
}
// Exclusive => Upgrade
void UpgradeMutex::unlock_and_lock_upgrade() {
  std::lock_guard _{_m};
  TRI_ASSERT(writer());
  TRI_ASSERT(!upgrader());
  TRI_ASSERT(readers() == 0);
  _state = kUpgradableEntered | 1;
  // notify all since multiple *lock_shared*() calls may be able
  // to proceed in response to this notification
  _gate1.notify_all();
}

// Upgrade => Shared
void UpgradeMutex::unlock_upgrade_and_lock_shared() {
  std::lock_guard _{_m};
  TRI_ASSERT(!writer());
  TRI_ASSERT(upgrader());
  TRI_ASSERT(readers() > 0);
  _state &= ~kUpgradableEntered;
  // notify all since only one *lock*() or *lock_upgrade*() call can win and
  // proceed in response to this notification, but a *lock_shared*() call may
  // also be waiting and could steal the notification
  _gate1.notify_all();
}
// Upgrade => Exclusive
void UpgradeMutex::unlock_upgrade_and_lock() {
  std::unique_lock lock{_m};
  TRI_ASSERT(!writer());
  TRI_ASSERT(upgrader());
  TRI_ASSERT(readers() > 0);
  size_t num_readers = (_state & kReaders) - 1;
  _state &= ~(kUpgradableEntered | kReaders);
  _state |= kWriteEntered | num_readers;
  _gate2.wait(lock, [this] { return readers() == 0; });
}
bool UpgradeMutex::try_unlock_upgrade_and_lock() {
  std::unique_lock lock{_m};
  TRI_ASSERT(!writer());
  TRI_ASSERT(upgrader());
  TRI_ASSERT(readers() > 0);
  if (readers() != 1) {
    return false;
  }
  _state = kWriteEntered;
  return true;
}
bool UpgradeMutex::writer() const noexcept {
  return (_state & kWriteEntered) != 0;
}
size_t UpgradeMutex::readers() const noexcept { return _state & kReaders; }
bool UpgradeMutex::upgrader() const noexcept {
  return (_state & kUpgradableEntered) != 0;
}
bool UpgradeMutex::no_writer_no_max_readers() const noexcept {
  return !writer() && readers() != kReaders;
}
bool UpgradeMutex::no_writer_no_upgrader() const noexcept {
  return (_state & (kWriteEntered | kUpgradableEntered)) == 0;
}
bool UpgradeMutex::no_writer_no_upgrader_no_max_readers() const noexcept {
  return no_writer_no_upgrader() && readers() != kReaders;
}

}  // namespace arangodb::basics
