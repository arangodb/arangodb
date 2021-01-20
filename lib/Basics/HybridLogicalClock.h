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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_HYBRID_LOGICAL_CLOCK_H
#define ARANGODB_BASICS_HYBRID_LOGICAL_CLOCK_H 1

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>

#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "Basics/Common.h"

namespace arangodb {
namespace basics {

class HybridLogicalClock {
 public:
  typedef std::chrono::high_resolution_clock ClockT;

 private:
  ClockT _clock;
  std::atomic<uint64_t> _lastTimeStamp;
  uint64_t _offset1970;

 public:
  HybridLogicalClock() : _lastTimeStamp(0), _offset1970(computeOffset1970()) {}
  HybridLogicalClock(HybridLogicalClock const& other) = delete;
  HybridLogicalClock(HybridLogicalClock&& other) = delete;
  HybridLogicalClock& operator=(HybridLogicalClock const& other) = delete;
  HybridLogicalClock& operator=(HybridLogicalClock&& other) = delete;

  uint64_t getTimeStamp() {
    uint64_t oldTimeStamp;
    uint64_t newTimeStamp;
    do {
      uint64_t physical = getPhysicalTime();
      oldTimeStamp = _lastTimeStamp.load(std::memory_order_relaxed);
      uint64_t oldTime = extractTime(oldTimeStamp);
      newTimeStamp = (physical <= oldTime)
                         ? assembleTimeStamp(oldTime, extractCount(oldTimeStamp) + 1)
                         : assembleTimeStamp(physical, 0);
    } while (!_lastTimeStamp.compare_exchange_weak(oldTimeStamp, newTimeStamp, std::memory_order_release,
                                                   std::memory_order_relaxed));
    return newTimeStamp;
  }

  // Call the following when a message with a time stamp has been received:
  uint64_t getTimeStamp(uint64_t receivedTimeStamp) {
    uint64_t oldTimeStamp;
    uint64_t newTimeStamp;
    do {
      uint64_t physical = getPhysicalTime();
      oldTimeStamp = _lastTimeStamp.load(std::memory_order_relaxed);
      uint64_t oldTime = extractTime(oldTimeStamp);
      uint64_t recTime = extractTime(receivedTimeStamp);
      uint64_t newTime = (std::max)((std::max)(oldTime, physical), recTime);
      // Note that this implies newTime >= oldTime and newTime >= recTime
      uint64_t newCount;
      if (newTime == oldTime) {
        if (newTime == recTime) {
          // all three identical
          newCount =
              (std::max)(extractCount(oldTimeStamp), extractCount(receivedTimeStamp)) + 1;
        } else {
          // this means recTime < newTime
          newCount = extractCount(oldTimeStamp) + 1;
        }
      } else {
        // newTime > oldTime
        if (newTime == recTime) {
          newCount = extractCount(receivedTimeStamp) + 1;
        } else {
          newCount = 0;
        }
      }
      newTimeStamp = assembleTimeStamp(newTime, newCount);
    } while (!_lastTimeStamp.compare_exchange_weak(oldTimeStamp, newTimeStamp, std::memory_order_release,
                                                   std::memory_order_relaxed));
    return newTimeStamp;
  }

  /// encodes the uint64_t timestamp into a new string
  static std::string encodeTimeStamp(uint64_t t) {
    std::string r(11, '\x00');
    size_t pos = 11;
    while (t > 0) {
      r[--pos] = encodeTable[static_cast<uint8_t>(t & 0x3ful)];
      t >>= 6;
    }
    return r.substr(pos, 11 - pos);
  }

  /// encodes the uint64_t timestamp into the provided result buffer
  /// the result buffer must be at least 11 chars long
  /// the length of the encoded value and the start position into
  /// the result buffer are returned
  static std::pair<size_t, size_t> encodeTimeStamp(uint64_t t, char* r) {
    size_t pos = 11;
    while (t > 0) {
      r[--pos] = encodeTable[static_cast<uint8_t>(t & 0x3ful)];
      t >>= 6;
    }
    return std::make_pair(pos, 11 - pos);
  }

  static velocypack::ValuePair encodeTimeStampToValuePair(uint64_t t, char* r) {
    auto p = encodeTimeStamp(t, r);
    return velocypack::ValuePair(&r[0] + p.first, p.second, velocypack::ValueType::String);
  }

  static uint64_t decodeTimeStamp(std::string const& s) {
    return decodeTimeStamp(s.data(), s.size());
  }

  static uint64_t decodeTimeStamp(velocypack::Slice const& s) {
    if (!s.isString()) {
      return std::numeric_limits<std::uint64_t>::max();
    }
    velocypack::ValueLength l;
    char const* p = s.getString(l);
    return decodeTimeStamp(p, l);
  }

  static uint64_t decodeTimeStamp(char const* p, size_t len) {
    // Returns UINT64_MAX if format is not valid
    if (len > 11) {
      return UINT64_MAX;
    }
    uint64_t r = 0;
    for (size_t i = 0; i < len; i++) {
      signed char c = decodeTable[static_cast<uint8_t>(p[i])];
      if (c < 0) {
        return UINT64_MAX;
      }
      r = (r << 6) | static_cast<uint8_t>(c);
    }
    return r;
  }

  // helper to get the physical time in milliseconds since the epoch:
  uint64_t getPhysicalTime() {
    auto now = _clock.now();
    uint64_t ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
            .count() -
        _offset1970;
    return ms;
  }

  // helper to compute the offset between epoch and 1970
  uint64_t computeOffset1970();

  static uint64_t extractTime(uint64_t t) { return t >> 20; }

  static uint64_t extractCount(uint64_t t) { return t & 0xfffffUL; }

  static uint64_t assembleTimeStamp(uint64_t time, uint64_t count) {
    return (time << 20) + count;
  }

 private:
  static char encodeTable[65];

  static signed char decodeTable[256];
};
}  // namespace basics
}  // namespace arangodb

#endif
