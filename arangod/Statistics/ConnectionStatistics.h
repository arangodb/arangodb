////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Statistics/StatisticsFeature.h"
#include "Statistics/figures.h"

namespace arangodb {
class ConnectionStatistics {
 public:
  static uint64_t memoryUsage() noexcept;
  static void initialize();

  class Item {
   public:
    constexpr Item() noexcept : _stat(nullptr) {}
    explicit Item(ConnectionStatistics* stat) noexcept : _stat(stat) {}

    Item(Item const&) = delete;
    Item& operator=(Item const&) = delete;

    Item(Item&& r) noexcept : _stat(r._stat) { r._stat = nullptr; }
    Item& operator=(Item&& r) noexcept {
      if (&r != this) {
        reset();
        _stat = r._stat;
        r._stat = nullptr;
      }
      return *this;
    }

    ~Item() { reset(); }

    void reset() noexcept {
      if (_stat != nullptr) {
        _stat->release();
        _stat = nullptr;
      }
    }

    void SET_START() {
      if (_stat != nullptr) {
        _stat->_connStart = StatisticsFeature::time();
      }
    }

    void SET_END() {
      if (_stat != nullptr) {
        _stat->_connEnd = StatisticsFeature::time();
      }
    }

    void SET_HTTP();

   private:
    ConnectionStatistics* _stat;
  };

  ConnectionStatistics() noexcept { reset(); }
  static Item acquire() noexcept;

  struct Snapshot {
    statistics::Counter httpConnections;
    statistics::Counter totalRequests;
    statistics::Counter totalRequestsSuperuser;
    statistics::Counter totalRequestsUser;
    statistics::MethodRequestCounters methodRequests;
    statistics::Counter asyncRequests;
    statistics::Distribution connectionTime;
  };

  static void getSnapshot(Snapshot& snapshot);

 private:
  void release() noexcept;

  void reset() noexcept {
    _connStart = 0.0;
    _connEnd = 0.0;
    _http = false;
    _error = false;
  }

 private:
  double _connStart;
  double _connEnd;

  bool _http;
  bool _error;
};
}  // namespace arangodb
