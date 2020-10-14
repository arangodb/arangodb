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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STATISTICS_CONNECTION_STATISTICS_H
#define ARANGOD_STATISTICS_CONNECTION_STATISTICS_H 1

#include "Basics/Common.h"

#include "Statistics/StatisticsFeature.h"
#include "Statistics/figures.h"

namespace arangodb {
class ConnectionStatistics {
 public:
  static void initialize();
  
  class Item {
   public:
    Item() : _stat(nullptr) {}
    explicit Item(ConnectionStatistics* stat) : _stat(stat) {}

    Item(Item const&) = delete;
    Item& operator=(Item const&) = delete;

    Item(Item&& r) : _stat(r._stat) { r._stat = nullptr; }
    Item& operator=(Item&& r) {
      if (&r != this) {
        reset();
        _stat = r._stat;
        r._stat = nullptr;
      }
      return *this;
    }

    ~Item() { reset(); }

    void reset() {
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

    void SET_HTTP();

   private:
    ConnectionStatistics* _stat;
  };
  
  static Item acquire();

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
  ConnectionStatistics() { reset(); }

  void release();

  void reset() {
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

#endif
