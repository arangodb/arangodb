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

#ifndef ARANGOD_STATISTICS_STATISTICS_H
#define ARANGOD_STATISTICS_STATISTICS_H 1

#include "Basics/Common.h"
#include "Rest/HttpRequest.h"
#include "Statistics/figures.h"

struct TRI_request_statistics_t {
#ifdef USE_DEV_TIMERS
  static thread_local TRI_request_statistics_t* STATS;
#endif

  TRI_request_statistics_t()
      : _readStart(0.0),
        _readEnd(0.0),
        _queueStart(0.0),
        _queueEnd(0.0),
        _requestStart(0.0),
        _requestEnd(0.0),
        _writeStart(0.0),
        _writeEnd(0.0),
        _receivedBytes(0.0),
        _sentBytes(0.0),
        _requestType(arangodb::rest::RequestType::ILLEGAL),
        _async(false),
        _tooLarge(false),
        _executeError(false),
        _ignore(false) {
#ifdef USE_DEV_TIMERS
    _id = nullptr;
#endif
  }

  void reset() {
    _readStart = 0.0;
    _readEnd = 0.0;
    _queueStart = 0.0;
    _queueEnd = 0.0;
    _requestStart = 0.0;
    _requestEnd = 0.0;
    _writeStart = 0.0;
    _writeEnd = 0.0;
    _receivedBytes = 0.0;
    _sentBytes = 0.0;
    _requestType = arangodb::rest::RequestType::ILLEGAL;
    _async = false;
    _tooLarge = false;
    _executeError = false;
    _ignore = false;
#ifdef USE_DEV_TIMERS
    _sections.clear();
    _timings.clear();
#endif
  }

  double _readStart;     // CommTask::processRead - read first byte of message
  double _readEnd;       // CommTask::processRead - message complete
  double _queueStart;    // addJob to queue GeneralServer::handleRequest
  double _queueEnd;      // exit queue DispatcherThread::handleJob
  double _requestStart;  // GeneralServerJob::work
  double _requestEnd;
  double _writeStart;
  double _writeEnd;

  double _receivedBytes;
  double _sentBytes;

  arangodb::rest::RequestType _requestType;

  bool _async;
  bool _tooLarge;
  bool _executeError;
  bool _ignore;

#ifdef USE_DEV_TIMERS
  void* _id;
  std::vector<std::string> _sections;
  std::vector<double> _timings;
#endif
};

struct TRI_connection_statistics_t {
  TRI_connection_statistics_t()
      : _connStart(0.0), _connEnd(0.0), _http(false), _error(false) {}

  void reset() {
    _connStart = 0.0;
    _connEnd = 0.0;
    _http = false;
    _error = false;
  }

  double _connStart;
  double _connEnd;

  bool _http;
  bool _error;
};

struct TRI_server_statistics_t {
  TRI_server_statistics_t() : _startTime(0.0), _uptime(0.0) {}

  double _startTime;
  double _uptime;
};

TRI_request_statistics_t* TRI_AcquireRequestStatistics(void);
void TRI_ReleaseRequestStatistics(TRI_request_statistics_t*);

void TRI_FillRequestStatistics(
    arangodb::basics::StatisticsDistribution& totalTime,
    arangodb::basics::StatisticsDistribution& requestTime,
    arangodb::basics::StatisticsDistribution& queueTime,
    arangodb::basics::StatisticsDistribution& ioTime,
    arangodb::basics::StatisticsDistribution& bytesSent,
    arangodb::basics::StatisticsDistribution& bytesReceived);

TRI_connection_statistics_t* TRI_AcquireConnectionStatistics(void);
void TRI_ReleaseConnectionStatistics(TRI_connection_statistics_t*);

void TRI_FillConnectionStatistics(
    arangodb::basics::StatisticsCounter& httpConnections,
    arangodb::basics::StatisticsCounter& totalRequests,
    std::vector<arangodb::basics::StatisticsCounter>& methodRequests,
    arangodb::basics::StatisticsCounter& asyncRequests,
    arangodb::basics::StatisticsDistribution& connectionTime);

TRI_server_statistics_t TRI_GetServerStatistics();

extern arangodb::basics::StatisticsCounter TRI_HttpConnectionsStatistics;
extern arangodb::basics::StatisticsCounter TRI_TotalRequestsStatistics;
extern std::vector<arangodb::basics::StatisticsCounter>
    TRI_MethodRequestsStatistics;
extern arangodb::basics::StatisticsCounter TRI_AsyncRequestsStatistics;
extern arangodb::basics::StatisticsVector
    TRI_ConnectionTimeDistributionVectorStatistics;
extern arangodb::basics::StatisticsDistribution*
    TRI_ConnectionTimeDistributionStatistics;
extern arangodb::basics::StatisticsVector
    TRI_RequestTimeDistributionVectorStatistics;
extern arangodb::basics::StatisticsDistribution*
    TRI_TotalTimeDistributionStatistics;
extern arangodb::basics::StatisticsDistribution*
    TRI_RequestTimeDistributionStatistics;
extern arangodb::basics::StatisticsDistribution*
    TRI_QueueTimeDistributionStatistics;
extern arangodb::basics::StatisticsDistribution*
    TRI_IoTimeDistributionStatistics;
extern arangodb::basics::StatisticsVector
    TRI_BytesSentDistributionVectorStatistics;
extern arangodb::basics::StatisticsDistribution*
    TRI_BytesSentDistributionStatistics;
extern arangodb::basics::StatisticsVector
    TRI_BytesReceivedDistributionVectorStatistics;
extern arangodb::basics::StatisticsDistribution*
    TRI_BytesReceivedDistributionStatistics;
extern TRI_server_statistics_t TRI_ServerStatistics;

inline double TRI_StatisticsTime() { return TRI_microtime(); }

void TRI_InitializeStatistics();

#endif
