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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SOCKET_TASK_H
#define ARANGOD_SCHEDULER_SOCKET_TASK_H 1

#include "Scheduler/Task.h"

#include <boost/asio/ssl.hpp>

#include "Basics/Mutex.h"
#include "Basics/StringBuffer.h"
#include "Basics/asio-helper.h"
#include "Scheduler/Socket.h"
#include "Statistics/StatisticsAgent.h"

namespace arangodb {
namespace rest {
class SocketTask : virtual public Task, public ConnectionStatisticsAgent {
  friend class HttpCommTask;
  explicit SocketTask(SocketTask const&) = delete;
  SocketTask& operator=(SocketTask const&) = delete;

 private:
  static size_t const READ_BLOCK_SIZE = 10000;

 public:
  SocketTask(EventLoop, std::unique_ptr<Socket>, ConnectionInfo&&,
             double keepAliveTimeout, bool skipInit);

  virtual ~SocketTask();

  std::unique_ptr<Socket> releasePeer() {
    _abandoned = true;
    return std::move(_peer);
  }

  ConnectionInfo&& releaseConnectionInfo() {
    _abandoned = true;
    return std::move(_connectionInfo);
  }

 public:
  void start();

 protected:
  virtual bool processRead(double start_time) = 0;

  // This function is used during the protocol switch from http
  // to VelocyStream. This way we no not require additional
  // constructor arguments. It should not be used otherwise.
  void addToReadBuffer(char const* data, std::size_t len);

 protected:
  void addWriteBuffer(std::unique_ptr<basics::StringBuffer> buffer) {
    addWriteBuffer(std::move(buffer), (RequestStatisticsAgent*)nullptr);
  }

  void addWriteBuffer(std::unique_ptr<basics::StringBuffer>,
                      RequestStatisticsAgent*);

  void addWriteBuffer(basics::StringBuffer*, TRI_request_statistics_t*);

  void completedWriteBuffer();

  void closeStream();

  void resetKeepAlive();
  void cancelKeepAlive();

 protected:
  ConnectionInfo _connectionInfo;

  basics::StringBuffer _readBuffer;

 private:
  Mutex _writeLock;
  basics::StringBuffer* _writeBuffer = nullptr;
  TRI_request_statistics_t* _writeBufferStatistics = nullptr;

  std::deque<basics::StringBuffer*> _writeBuffers;
  std::deque<TRI_request_statistics_t*> _writeBuffersStats;

 protected:
  std::unique_ptr<Socket> _peer;
  boost::posix_time::milliseconds _keepAliveTimeout;
  boost::asio::deadline_timer _keepAliveTimer;
  bool const _useKeepAliveTimer;
  bool _keepAliveTimerActive;
  bool _closeRequested;
  std::atomic_bool _abandoned;

 private:
  bool reserveMemory();
  bool trySyncRead();
  void asyncReadSome();
  void closeReceiveStream();

 private:
  bool _closedSend = false;
  bool _closedReceive = false;
};
}
}

#endif
