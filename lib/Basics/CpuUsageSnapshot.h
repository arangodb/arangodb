////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_CPU_USAGE_SNAPSHOT_H
#define ARANGODB_CPU_USAGE_SNAPSHOT_H 1

#include <cstddef>
#include <cstdint>

namespace arangodb {

/// @brief simple struct to store a single CPU usage snapshot.
/// there are different slots for user time, nice time, system time, etc.
/// all values are supposed to be measured in units of USER_HZ 
/// (1/100ths of a second on most architectures, use sysconf(_SC_CLK_TCK) 
/// to obtain the right value), at least that is the unit that /proc
/// will report for them.
struct CpuUsageSnapshot {
  /// @brief create a CpuUsageSnapshot from the contents of /proc/stat
  /// expects an input buffer with 10 numbers, each seperated by a space
  /// characters
  static CpuUsageSnapshot fromString(char const* buffer, std::size_t bufferSize) noexcept;

  /// @brief subtract the values of another CpuUsageSnapshot from ourselves
  void subtract(CpuUsageSnapshot const& other) noexcept;

  /// @brief whether or not the snapshot contains valid data.
  /// this is false for empty snapshots
  bool valid() const noexcept;

  /// @brief clear/invalidate a snapshot
  void clear() noexcept;

  /// @brief return total CPU time spent in the snapshot, including idle time 
  uint64_t total() const noexcept;

  /// @brief percent of user time (plus nice time) in ratio to total CPU time
  double userPercent() const noexcept {
    uint64_t t = total();
    return (t > 0) ? (100.0 * double(user + nice) / double(t)) : 0.0;
  }
  
  /// @brief percent of system time in ratio to total CPU time
  double systemPercent() const noexcept {
    uint64_t t = total();
    return (t > 0) ? (100.0 * double(system) / double(t)) : 0.0;
  }
  
  /// @brief percent of idle time in ratio to total CPU time
  double idlePercent() const noexcept {
    uint64_t t = total();
    return (t > 0) ? (100.0 * double(idle) / double(t)) : 0.0;
  }
  
  /// @brief percent of io wait time in ratio to total CPU time
  double iowaitPercent() const noexcept {
    uint64_t t = total();
    return (t > 0) ? (100.0 * double(iowait) / double(t)) : 0.0;
  }

  /// @brief from `man proc`:
  ///  The amount of time, measured in units of USER_HZ (1/100ths of a second on most architectures, use sysconf(_SC_CLK_TCK) to obtain the  right  value),  that  the
  ///  system ("cpu" line) or the specific CPU ("cpuN" line) spent in various states:
  ///    user       (1) Time spent in user mode.
  ///    nice       (2) Time spent in user mode with low priority (nice).
  ///    system     (3) Time spent in system mode.
  ///    idle       (4) Time spent in the idle task.  This value should be USER_HZ times the second entry in the /proc/uptime pseudo-file.
  ///    iowait     (5) Time waiting for I/O to complete.  This value is not reliable, for the following reasons:
  ///    irq        (6) Time servicing interrupts.
  ///    softirq    (7) Time servicing softirqs.
  ///    steal      (8) Stolen time, which is the time spent in other operating systems when running in a virtualized environment
  ///    guest      (9) Time spent running a virtual CPU for guest operating systems under the control of the Linux kernel.
  ///    guest_nice (10) Time spent running a niced guest (virtual CPU for guest operating systems under the control of the Linux kernel).
  ///  One Windows, only user, system and idle are used, where "system" is the time spent in kernel mode.
  uint64_t user = 0;
  uint64_t nice = 0;
  uint64_t system = 0;
  uint64_t idle = 0;
  uint64_t iowait = 0;
  uint64_t irq = 0;
  uint64_t softirq = 0;
  uint64_t steal = 0;
  uint64_t guest = 0;
  uint64_t guestnice = 0;
};

}  // namespace arangodb

#endif
