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
#include <list>

#include "Basics/Mutex.h"
#include "Basics/SmallVector.h"
#include "Basics/StringBuffer.h"
#include "Basics/asio-helper.h"
#include "Endpoint/ConnectionInfo.h"
#include "Scheduler/Socket.h"
#include "Statistics/RequestStatistics.h"

namespace arangodb {
class ConnectionStatistics;

namespace rest {
class SocketTask : virtual public Task {
  friend class HttpCommTask;

  explicit SocketTask(SocketTask const&) = delete;
  SocketTask& operator=(SocketTask const&) = delete;

 private:
  static size_t const READ_BLOCK_SIZE = 10000;

 public:
  SocketTask(EventLoop, std::unique_ptr<Socket>, ConnectionInfo&&,
             double keepAliveTimeout, bool skipInit);

  virtual ~SocketTask();

 public:
  void start();

 protected:
  // caller will hold the _lock
  virtual bool processRead(double startTime) = 0;
  virtual void compactify() {}

  // This function is used during the protocol switch from http
  // to VelocyStream. This way we no not require additional
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

    WriteBuffer(WriteBuffer&& other) 
        : _buffer(other._buffer), _statistics(other._statistics) {
      other._buffer = nullptr;
      other._statistics = nullptr;
    }

    WriteBuffer& operator=(WriteBuffer&& other) {
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

    bool empty() const {
      return _buffer == nullptr;
    }
    
    void clear() {
      _buffer = nullptr;
      _statistics = nullptr;
    }

    void release() {
      if (_buffer != nullptr) {
        delete _buffer;
        _buffer = nullptr;
      }

      if (_statistics != nullptr) {
        _statistics->release();
        _statistics = nullptr;
      }
    }
    
    void release(SocketTask* task) {
      if (_buffer != nullptr) {
        task->returnStringBuffer(_buffer);
        _buffer = nullptr;
      }

      if (_statistics != nullptr) {
        _statistics->release();
        _statistics = nullptr;
      }
    }
  };

  // will acquire the _lock
  void addWriteBuffer(WriteBuffer&);

  // will acquire the _lock
  void closeStream();
  
  // caller must hold the _lock
  void closeStreamNoLock();

  // caller must hold the _lock
  void resetKeepAlive();

  // caller must hold the _lock
  void cancelKeepAlive();
      
  basics::StringBuffer* leaseStringBuffer(size_t length);
  void returnStringBuffer(basics::StringBuffer*);
  
 protected:
  Mutex _lock;
  ConnectionStatistics* _connectionStatistics;
  ConnectionInfo _connectionInfo;
  basics::StringBuffer _readBuffer; // needs _lock
  
  SmallVector<basics::StringBuffer*, 32>::allocator_type::arena_type _stringBuffersArena;
  SmallVector<basics::StringBuffer*, 32> _stringBuffers; // needs _lock

 private:
  void writeWriteBuffer();
  bool completedWriteBuffer();

  bool reserveMemory();
  bool trySyncRead();
  bool processAll();
  void asyncReadSome();
  bool abandon();

 private:
  WriteBuffer _writeBuffer;
  std::list<WriteBuffer> _writeBuffers;

  std::unique_ptr<Socket> _peer;
  boost::posix_time::milliseconds _keepAliveTimeout;
  boost::asio::deadline_timer _keepAliveTimer;
  bool const _useKeepAliveTimer;
  bool _keepAliveTimerActive;
  bool _closeRequested;
  std::atomic<bool> _abandoned;

  bool _closedSend = false;
  bool _closedReceive = false;
};
}
}

#endif
