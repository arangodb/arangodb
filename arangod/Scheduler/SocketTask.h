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

#ifndef ARANGOD_SCHEDULER_SOCKET_TASK2_H
#define ARANGOD_SCHEDULER_SOCKET_TASK2_H 1

#include "Scheduler/Task.h"

#include <boost/asio/ssl.hpp>

#include "Basics/asio-helper.h"
#include "Basics/StringBuffer.h"
#include "Scheduler/Socket.h"
#include "Statistics/StatisticsAgent.h"

namespace arangodb {
namespace rest {
class SocketTask : virtual public Task, public ConnectionStatisticsAgent {
  explicit SocketTask(SocketTask const&) = delete;
  SocketTask& operator=(SocketTask const&) = delete;

 private:
  static size_t const READ_BLOCK_SIZE = 10000;

 public:
  SocketTask(EventLoop, std::unique_ptr<Socket>, ConnectionInfo&&,
             double keepAliveTimeout);

  virtual ~SocketTask();

 public:
  void start();

 protected:
  virtual bool processRead() = 0;

 protected:
  void addWriteBuffer(std::unique_ptr<basics::StringBuffer> buffer) {
    addWriteBuffer(std::move(buffer), (RequestStatisticsAgent*)nullptr);
  }

  void addWriteBuffer(std::unique_ptr<basics::StringBuffer>,
                      RequestStatisticsAgent*);

  void addWriteBuffer(basics::StringBuffer*, TRI_request_statistics_t*);

  void completedWriteBuffer();

  void closeStream();

 protected:
  ConnectionInfo _connectionInfo;

  basics::StringBuffer _readBuffer;

  basics::StringBuffer* _writeBuffer = nullptr;
  TRI_request_statistics_t* _writeBufferStatistics = nullptr;

  std::deque<basics::StringBuffer*> _writeBuffers;
  std::deque<TRI_request_statistics_t*> _writeBuffersStats;

  std::unique_ptr<Socket> _peer;
  bool _useKeepAliveTimeout;
  boost::posix_time::milliseconds _keepAliveTimeout;
  boost::asio::deadline_timer _keepAliveTimer;
  void resetKeepAlive(boost::system::error_code& ec);
  void cancelKeepAlive(boost::system::error_code& ec);
  bool cancelKeepAlive() {
    boost::system::error_code ec;
    cancelKeepAlive(ec);
    if (ec) {
      return false;
    }
    return true;
  }

 protected:
  bool _closeRequested = false;

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
