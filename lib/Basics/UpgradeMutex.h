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
#pragma once

/// Based on boost/1.78.0/boost/thread/v2/shared_mutex.hpp
/// Copyright Howard Hinnant 2007-2010.
/// Copyright Vicente J. Botet Escriba 2012.
///
///  Distributed under the Boost Software License, Version 1.0. (See
///  accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)

#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <climits>

namespace arangodb::basics {

// Simple, not scalable, correct upgrade mutex
// Not recursive
// Priority:
//  write more than read
//  write same as upgrade
//  upgrade not concurrent with read
class UpgradeMutex {
 public:
  UpgradeMutex();
  ~UpgradeMutex();

  // Exclusive ownership
  void lock();
  bool try_lock();
  void unlock();

  // Shared ownership
  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();

  // Upgrade ownership
  void lock_upgrade();
  bool try_lock_upgrade();
  void unlock_upgrade();

  // Exclusive => Shared
  void unlock_and_lock_shared();
  // Exclusive => Upgrade
  void unlock_and_lock_upgrade();

  // Shared => Exclusive
  // void unlock_shared_and_lock(); // can cause a deadlock if used
  // Shared => Upgrade
  // void unlock_shared_and_lock_upgrade(); // can cause a deadlock if used

  // Upgrade => Shared
  void unlock_upgrade_and_lock_shared();
  // Upgrade => Exclusive
  void unlock_upgrade_and_lock();
  bool try_unlock_upgrade_and_lock();

 private:
  std::mutex _m;
  std::condition_variable _gate1;
  // the _gate2 condition variable is only used by functions
  // that have taken kWriteEntered but are waiting for readers() == 0
  std::condition_variable _gate2;
  size_t _state;

  static constexpr size_t kWriteEntered = 1ULL
                                          << (sizeof(size_t) * CHAR_BIT - 1);
  static constexpr size_t kUpgradableEntered = kWriteEntered >> 1;
  static constexpr size_t kReaders = ~(kWriteEntered | kUpgradableEntered);

  // Helpers
  [[nodiscard]] bool writer() const noexcept;
  [[nodiscard]] size_t readers() const noexcept;
  [[nodiscard]] bool upgrader() const noexcept;
  [[nodiscard]] bool no_writer_no_max_readers() const noexcept;
  [[nodiscard]] bool no_writer_no_upgrader() const noexcept;
  [[nodiscard]] bool no_writer_no_upgrader_no_max_readers() const noexcept;
};

}  // namespace arangodb::basics
