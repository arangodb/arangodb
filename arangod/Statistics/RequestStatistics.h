////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_STATISTICS_REQUEST_STATISTICS_H
#define ARANGOD_STATISTICS_REQUEST_STATISTICS_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Rest/CommonDefines.h"
#include "Statistics/StatisticsFeature.h"
#include "Statistics/figures.h"

#include <boost/lockfree/queue.hpp>

namespace arangodb {
class RequestStatistics {
 public:
  static void initialize();
  static size_t processAll();

  static RequestStatistics* acquire();
  void release();

  static void SET_ASYNC(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_async = true;
    }
  }

  static void SET_IGNORE(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_ignore = true;
    }
  }

  static void SET_REQUEST_TYPE(RequestStatistics* stat, rest::RequestType t) {
    if (stat != nullptr) {
      stat->_requestType = t;
    }
  }

  static void SET_READ_START(RequestStatistics* stat, double start) {
    if (stat != nullptr) {
      if (stat->_readStart == 0.0) {
        stat->_readStart = start;
      }
    }
  }

  static void SET_READ_END(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_readEnd = StatisticsFeature::time();
    }
  }

  static void SET_WRITE_START(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_writeStart = StatisticsFeature::time();
    }
  }

  static void SET_WRITE_END(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_writeEnd = StatisticsFeature::time();
    }
  }

  static void SET_QUEUE_START(RequestStatistics* stat, int64_t nrQueued) {
    if (stat != nullptr) {
      stat->_queueStart = StatisticsFeature::time();
      stat->_queueSize = nrQueued;
    }
  }

  static void SET_QUEUE_END(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_queueEnd = StatisticsFeature::time();
    }
  }

  static void ADD_RECEIVED_BYTES(RequestStatistics* stat, size_t bytes) {
    if (stat != nullptr) {
      stat->_receivedBytes += bytes;
    }
  }

  static void ADD_SENT_BYTES(RequestStatistics* stat, size_t bytes) {
    if (stat != nullptr) {
      stat->_sentBytes += bytes;
    }
  }

  static void SET_EXECUTE_ERROR(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_executeError = true;
    }
  }

  static void SET_REQUEST_START(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_requestStart = StatisticsFeature::time();
    }
  }

  static void SET_REQUEST_END(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_requestEnd = StatisticsFeature::time();
    }
  }

  static void SET_REQUEST_START_END(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_requestStart = StatisticsFeature::time();
      stat->_requestEnd = StatisticsFeature::time();
    }
  }

  static double ELAPSED_SINCE_READ_START(RequestStatistics* stat) {
    if (stat != nullptr) {
      return StatisticsFeature::time() - stat->_readStart;
    } else {
      return 0.0;
    }
  }

  static void SET_SUPERUSER(RequestStatistics* stat) {
    if (stat != nullptr) {
      stat->_superuser = true;
    }
  }

  double requestStart() const { return _requestStart; }

  static void fill(basics::StatisticsDistribution& totalTime,
                   basics::StatisticsDistribution& requestTime,
                   basics::StatisticsDistribution& queueTime,
                   basics::StatisticsDistribution& ioTime,
                   basics::StatisticsDistribution& bytesSent,
                   basics::StatisticsDistribution& bytesReceived,
                   stats::RequestStatisticsSource source);

  std::string timingsCsv();
  std::string to_string();
  void trace_log();

 private:
  static size_t const QUEUE_SIZE = 64 * 1024 - 2;  // current (1.62) boost maximum

  static std::unique_ptr<RequestStatistics[]> _statisticsBuffer;

  static boost::lockfree::queue<RequestStatistics*, boost::lockfree::capacity<QUEUE_SIZE>> _freeList;

  static boost::lockfree::queue<RequestStatistics*, boost::lockfree::capacity<QUEUE_SIZE>> _finishedList;

  static void process(RequestStatistics*);

  RequestStatistics() { reset(); }

  void reset() {
    _readStart = 0.0;
    _readEnd = 0.0;
    _queueStart = 0.0;
    _queueEnd = 0.0;
    _queueSize = 0;
    _requestStart = 0.0;
    _requestEnd = 0.0;
    _writeStart = 0.0;
    _writeEnd = 0.0;
    _receivedBytes = 0.0;
    _sentBytes = 0.0;
    _requestType = rest::RequestType::ILLEGAL;
    _async = false;
    _tooLarge = false;
    _executeError = false;
    _ignore = false;
    _released = true;
    _inQueue = false;
    _superuser = false;
  }

  double _readStart;   // CommTask::processRead - read first byte of message
  double _readEnd;     // CommTask::processRead - message complete
  double _queueStart;  // job added to JobQueue
  double _queueEnd;    // job removed from JobQueue
  int64_t _queueSize;

  double _requestStart;  // GeneralServerJob::work
  double _requestEnd;
  double _writeStart;
  double _writeEnd;

  double _receivedBytes;
  double _sentBytes;

  rest::RequestType _requestType;

  bool _async;
  bool _tooLarge;
  bool _executeError;
  bool _ignore;
  bool _released;
  bool _inQueue;
  bool _superuser;
};
}  // namespace arangodb

#endif
