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

#include "Basics/StringBuffer.h"
#include "Basics/Thread.h"
#include "Basics/socket-utils.h"
#include "Statistics/StatisticsAgent.h"

namespace arangodb {
namespace rest {

class SocketTask : virtual public Task, public ConnectionStatisticsAgent {
  explicit SocketTask(SocketTask const&) = delete;
  SocketTask& operator=(SocketTask const&) = delete;

 private:
  static size_t const READ_BLOCK_SIZE = 10000;

 public:
  SocketTask(TRI_socket_t, ConnectionInfo&&, double);

 protected:
  ~SocketTask();

 public:
  void armKeepAliveTimeout();

 protected:
  virtual bool fillReadBuffer();

  virtual bool handleRead();   // called by handleEvent
  virtual bool handleWrite();  // called by handleEvent

  virtual void handleTimeout() = 0;

  // is called in a loop as long as it returns true.
  // Return false if there is not enough data to do
  // any more processing and all available data has
  // been evaluated.
  virtual bool processRead() = 0;

 protected:
  void addWriteBuffer(std::unique_ptr<basics::StringBuffer> buffer) {
    addWriteBuffer(std::move(buffer), (RequestStatisticsAgent*)nullptr);
  }

  void addWriteBuffer(std::unique_ptr<basics::StringBuffer>,
                      RequestStatisticsAgent*);

  void addWriteBuffer(basics::StringBuffer*, TRI_request_statistics_t*);

  void completedWriteBuffer();

 protected:
  bool setup(Scheduler*, EventLoop) override;
  void cleanup() override;
  bool handleEvent(EventToken token, EventType) override;

 protected:
  double const _keepAliveTimeout;

 protected:
  // connection closed
  bool _closed = false;

  // client has closed the connection, immediately close the underlying socket
  bool _clientClosed = false;

  // the client has requested, close the connection after all data is written
  bool _closeRequested = false;

 protected:
  TRI_socket_t _commSocket;
  ConnectionInfo _connectionInfo;

  basics::StringBuffer _readBuffer;

  basics::StringBuffer* _writeBuffer = nullptr;
  TRI_request_statistics_t* _writeBufferStatistics = nullptr;

  std::deque<std::pair<basics::StringBuffer*, TRI_request_statistics_t*>> _writeBuffers;

  EventToken _keepAliveWatcher = nullptr;
  EventToken _readWatcher = nullptr;
  EventToken _writeWatcher = nullptr;

  size_t _writeLength = 0;

  TRI_tid_t _tid = 0;
};
}
}

#endif
