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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STATISTICS_STATISTICS_AGENT_H
#define ARANGOD_STATISTICS_STATISTICS_AGENT_H 1

#include "Basics/Common.h"
#include "Meta/utility.h"

#include "Statistics/StatisticsFeature.h"
#include "Statistics/statistics.h"

namespace arangodb {
namespace rest {
template <typename STAT, typename FUNC>
class StatisticsAgent {
  StatisticsAgent(StatisticsAgent const&) = delete;
  StatisticsAgent& operator=(StatisticsAgent const&) = delete;

 public:
  StatisticsAgent(bool standalone = false)
      : _statistics(standalone ? FUNC::acquire() : nullptr),
        _lastReadStart(0.0) {}

  virtual ~StatisticsAgent() {
    if (_statistics != nullptr) {
      FUNC::release(_statistics);
    }
  }

 public:
  STAT* acquire() {
    if (_statistics != nullptr) {
      return _statistics;
    }

    _lastReadStart = 0.0;
    return _statistics = FUNC::acquire();
  }

  void release() {
    if (_statistics != nullptr) {
      FUNC::release(_statistics);
      _statistics = nullptr;
    }
  }

  void transferTo(StatisticsAgent* agent) {
    agent->replace(_statistics);
    _statistics = nullptr;
  }

  STAT* steal() {
    STAT* statistics = _statistics;
    _statistics = nullptr;

    return statistics;
  }

  double elapsedSinceReadStart() {
    if (_lastReadStart != 0.0) {
      return TRI_StatisticsTime() - _lastReadStart;
    }

    return 0.0;
  }

 public:
  STAT* _statistics;
  double _lastReadStart;

 protected:
  void replace(STAT* statistics) {
    if (_statistics != nullptr) {
      FUNC::release(_statistics);
    }

    _statistics = statistics;
  }
};

struct RequestStatisticsAgentDesc {
  static TRI_request_statistics_t* acquire() {
    return TRI_AcquireRequestStatistics();
  }

  static void release(TRI_request_statistics_t* stat) {
    TRI_ReleaseRequestStatistics(stat);
  }
};

class RequestStatisticsAgent
    : public StatisticsAgent<TRI_request_statistics_t,
                             RequestStatisticsAgentDesc> {
 public:
  RequestStatisticsAgent(bool standalone = false)
      : StatisticsAgent(standalone) {}

  RequestStatisticsAgent(RequestStatisticsAgent const&) = delete;

  RequestStatisticsAgent(RequestStatisticsAgent&& other) noexcept {
    _statistics = other._statistics;
    other._statistics = nullptr;

    _lastReadStart = other._lastReadStart;
    other._lastReadStart = 0.0;
  }
  
  void requestStatisticsAgentSetRequestType(rest::RequestType b) {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_requestType = b;
      }
    }
  }

  void requestStatisticsAgentSetAsync() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_async = true;
      }
    }
  }

  void requestStatisticsAgentSetReadStart() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr && _statistics->_readStart == 0.0) {
        _lastReadStart = _statistics->_readStart = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetReadEnd() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_readEnd = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetWriteStart() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_writeStart = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetWriteEnd() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_writeEnd = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetQueueStart() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_queueStart = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetQueueEnd() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_queueEnd = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetRequestStart() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_requestStart = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetRequestEnd() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_requestEnd = TRI_StatisticsTime();
      }
    }
  }

  void requestStatisticsAgentSetExecuteError() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_executeError = true;
      }
    }
  }

  void requestStatisticsAgentSetIgnore() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_ignore = true;
      }
    }
  }

  void requestStatisticsAgentAddReceivedBytes(size_t b) {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_receivedBytes += b;
      }
    }
  }

  void requestStatisticsAgentAddSentBytes(size_t b) {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_sentBytes += b;
      }
    }
  }
};

struct ConnectionStatisticsAgentDesc {
  static TRI_connection_statistics_t* acquire() {
    return TRI_AcquireConnectionStatistics();
  }

  static void release(TRI_connection_statistics_t* stat) {
    TRI_ReleaseConnectionStatistics(stat);
  }
};

class ConnectionStatisticsAgent
    : public StatisticsAgent<TRI_connection_statistics_t,
                             ConnectionStatisticsAgentDesc> {
 public:
  ConnectionStatisticsAgent() {
    acquire();
    connectionStatisticsAgentSetStart();
  }

  virtual ~ConnectionStatisticsAgent() {
    connectionStatisticsAgentSetEnd();
    release();
  }

 public:
  void connectionStatisticsAgentSetHttp() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_http = true;
        TRI_HttpConnectionsStatistics.incCounter();
      }
    }
  }

  // TODO FIXME -- modify statistics to respect vpp
  void connectionStatisticsAgentSetVpp() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_http = true;
        TRI_HttpConnectionsStatistics.incCounter();
      }
    }
  }

  void connectionStatisticsAgentSetStart() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_connStart = TRI_StatisticsTime();
      }
    }
  }

  void connectionStatisticsAgentSetEnd() {
    if (StatisticsFeature::enabled()) {
      if (_statistics != nullptr) {
        _statistics->_connEnd = TRI_StatisticsTime();
      }
    }
  }
};
}
}

#endif
