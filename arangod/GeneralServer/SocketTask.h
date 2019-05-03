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

#include "GeneralServer/Task.h"

#include "Basics/Mutex.h"
#include "Basics/SmallVector.h"
#include "Basics/StringBuffer.h"
#include "Endpoint/ConnectionInfo.h"
#include "GeneralServer/Socket.h"
#include "Statistics/RequestStatistics.h"

#include "GeneralServer/IoTask.h"

#include <list>

namespace arangodb {
class ConnectionStatistics;

namespace rest {
class SocketTask : public IoTask {
  friend class HttpCommTask;

  explicit SocketTask(SocketTask const&) = delete;
  SocketTask& operator=(SocketTask const&) = delete;

 private:
  static size_t const READ_BLOCK_SIZE = 10000;

 public:
  SocketTask(GeneralServer& server, GeneralServer::IoContext& context,
             char const* name,
             std::unique_ptr<Socket>, ConnectionInfo&&, double keepAliveTimeout,
             bool skipInit);

  virtual ~SocketTask();

 public:
  bool start();

  // whether or not this task can mix sync and async I/O
  virtual bool canUseMixedIO() const = 0;

 protected:
  // caller will hold the _lock
  virtual bool processRead(double startTime) = 0;
  virtual void compactify() {}

  // This function is used during the protocol switch from http
  // to VelocyStream. This way we do not require additional
  // constructor arguments. It should not be used otherwise.
  void addToReadBuffer(char const* data, std::size_t len);

 protected:
  struct WriteBuffer {
    basics::StringBuffer* _buffer;
    RequestStatistics* _statistics;

    WriteBuffer(basics::StringBuffer* buffer, RequestStatistics* statistics)
        : _buffer(buffer), _statistics(statistics) {}

    WriteBuffer(WriteBuffer const&) = delete;
    WriteBuffer& operator=(WriteBuffer const&) = delete;

    WriteBuffer(WriteBuffer&& other) noexcept
        : _buffer(other._buffer), _statistics(other._statistics) {
      other._buffer = nullptr;
      other._statistics = nullptr;
    }

    WriteBuffer& operator=(WriteBuffer&& other) noexcept {
      if (this != &other) {
        // release our own memory to prevent memleaks
        release();

        // take over ownership from other
        _buffer = other._buffer;
        _statistics = other._statistics;
        // fix other
        other._buffer = nullptr;
        other._statistics = nullptr;
      }
      return *this;
    }

    ~WriteBuffer() { release(); }

    bool empty() const noexcept { return _buffer == nullptr; }

    void clear() noexcept {
      _buffer = nullptr;
      _statistics = nullptr;
    }

    void release(SocketTask* task = nullptr) {
      if (_buffer != nullptr) {
        if (task != nullptr) {
          task->returnStringBuffer(_buffer);
        } else {
          delete _buffer;
        }
        _buffer = nullptr;
      }

      if (_statistics != nullptr) {
        _statistics->release();
        _statistics = nullptr;
      }
    }
  };

  // will be run in strand
  void addWriteBuffer(WriteBuffer&&);

  // will be run in strand
  void closeStream();

  // caller must run in _peer->strand()
  void closeStreamNoLock();

  // starts the keep alive time, no need to run on strand
  void resetKeepAlive();

  // cancels the keep alive timer
  void cancelKeepAlive();

  // abandon the task. if the task was already abandoned, this
  // method returns false. if abandoing was successful, this
  // method returns true. Used for VST upgrade
  bool abandon() { return !(_abandoned.exchange(true)); }

  // lease a string buffer from pool
  basics::StringBuffer* leaseStringBuffer(size_t length);
  void returnStringBuffer(basics::StringBuffer*);

  bool processAll();
  void triggerProcessAll();

 private:
  bool completedWriteBuffer();

  bool reserveMemory();
  bool trySyncRead();

  void asyncReadSome();
  void asyncWriteSome();

 protected:
  std::unique_ptr<Socket> _peer;
  ConnectionInfo _connectionInfo;

  ConnectionStatistics* _connectionStatistics;
  basics::StringBuffer _readBuffer;

 private:
  Mutex _bufferLock;
  SmallVector<basics::StringBuffer*, 32>::allocator_type::arena_type _stringBuffersArena;
  SmallVector<basics::StringBuffer*, 32> _stringBuffers;  // needs _bufferLock

  WriteBuffer _writeBuffer;
  std::list<WriteBuffer> _writeBuffers;

  boost::posix_time::milliseconds _keepAliveTimeout;
  std::unique_ptr<asio_ns::deadline_timer> _keepAliveTimer;
  bool const _useKeepAliveTimer;

  std::atomic<bool> _keepAliveTimerActive;
  std::atomic<bool> _closeRequested;

  std::atomic<bool> _abandoned;      // was task abandoned for another task
  std::atomic<bool> _closedSend;     // Close socket send
  std::atomic<bool> _closedReceive;  // Closed socket received
};
}  // namespace rest
}  // namespace arangodb

#endif
