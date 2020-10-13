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

#ifndef ARANGOD_STATISTICS_REQUEST_STATISTICS_H
#define ARANGOD_STATISTICS_REQUEST_STATISTICS_H 1

#include "Basics/Common.h"

#include "Rest/CommonDefines.h"
#include "Statistics/Descriptions.h"
#include "Statistics/StatisticsFeature.h"
#include "Statistics/figures.h"

namespace arangodb {
class RequestStatistics {
 public:
  static void initialize();
  static size_t processAll();

  class Item {
   public:
    Item() : _stat(nullptr) {}
    explicit Item(RequestStatistics* stat) : _stat(stat) {}

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

    operator bool() const { return _stat != nullptr; }

    void SET_ASYNC() const {
      if (_stat != nullptr) {
        _stat->_async = true;
      }
    }

    void SET_REQUEST_TYPE(rest::RequestType t) const {
      if (_stat != nullptr) {
        _stat->_requestType = t;
      }
    }

    void SET_READ_START(double start) const {
      if (_stat != nullptr) {
        if (_stat->_readStart == 0.0) {
          _stat->_readStart = start;
        }
      }
    }

    void SET_READ_END() const {
      if (_stat != nullptr) {
        _stat->_readEnd = StatisticsFeature::time();
      }
    }

    void SET_WRITE_START() const {
      if (_stat != nullptr) {
        _stat->_writeStart = StatisticsFeature::time();
      }
    }

    void SET_WRITE_END() const {
      if (_stat != nullptr) {
        _stat->_writeEnd = StatisticsFeature::time();
      }
    }

    void SET_QUEUE_START(int64_t nrQueued) const {
      if (_stat != nullptr) {
        _stat->_queueStart = StatisticsFeature::time();
        _stat->_queueSize = nrQueued;
      }
    }

    void SET_QUEUE_END() const {
      if (_stat != nullptr) {
        _stat->_queueEnd = StatisticsFeature::time();
      }
    }

    void ADD_RECEIVED_BYTES(size_t bytes) const {
      if (_stat != nullptr) {
        _stat->_receivedBytes += bytes;
      }
    }

    void ADD_SENT_BYTES(size_t bytes) const {
      if (_stat != nullptr) {
        _stat->_sentBytes += bytes;
      }
    }

    void SET_REQUEST_START() const {
      if (_stat != nullptr) {
        _stat->_requestStart = StatisticsFeature::time();
      }
    }

    void SET_REQUEST_END() const {
      if (_stat != nullptr) {
        _stat->_requestEnd = StatisticsFeature::time();
      }
    }

    void SET_REQUEST_START_END() const {
      if (_stat != nullptr) {
        _stat->_requestStart = StatisticsFeature::time();
        _stat->_requestEnd = StatisticsFeature::time();
      }
    }

    double ELAPSED_SINCE_READ_START() const {
      if (_stat != nullptr) {
        return StatisticsFeature::time() - _stat->_readStart;
      } else {
        return 0.0;
      }
    }

    void SET_SUPERUSER() const {
      if (_stat != nullptr) {
        _stat->_superuser = true;
      }
    }

    std::string timingsCsv() const;

   private:
     RequestStatistics* _stat;
  };

  static Item acquire();
  struct Snapshot {
    statistics::Distribution totalTime;
    statistics::Distribution requestTime;
    statistics::Distribution queueTime;
    statistics::Distribution ioTime;
    statistics::Distribution bytesSent;
    statistics::Distribution bytesReceived;
  };

  static void getSnapshot(Snapshot& snapshot, stats::RequestStatisticsSource source);

 private:
  static void process(RequestStatistics*);

  RequestStatistics() { reset(); }

  void release();

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
  bool _released;
  bool _inQueue;
  bool _superuser;
};
}  // namespace arangodb

#endif
