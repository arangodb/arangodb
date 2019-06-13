////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_SCHEDULER_SOCKET_H
#define ARANGOD_SCHEDULER_SOCKET_H 1

#include "Basics/Common.h"

#include "Basics/StringBuffer.h"
#include "Basics/asio_ns.h"
#include "GeneralServer/GeneralServer.h"
#include "Logger/Logger.h"

namespace arangodb {

typedef std::function<void(const asio_ns::error_code& ec, std::size_t transferred)> AsyncHandler;

class Socket {
 public:
  Socket(rest::GeneralServer::IoContext& context, bool encrypted)
      : _context(context), _encrypted(encrypted) {
    _context._clients.fetch_add(1, std::memory_order_release);
  }

  Socket(Socket const& that) = delete;
  Socket(Socket&& that) = delete;

  virtual ~Socket() {
    _context._clients.fetch_sub(1, std::memory_order_release);
  }

  bool isEncrypted() const { return _encrypted; }

  bool handshake() {
    if (!_encrypted || _handshakeDone) {
      return true;
    } else if (sslHandshake()) {
      _handshakeDone = true;
      return true;
    }
    return false;
  }

  void shutdown(asio_ns::error_code& ec, bool mustCloseSend, bool mustCloseReceive) {
    if (mustCloseSend) {
      this->shutdownSend(ec);
      if (ec && ec != asio_ns::error::not_connected) {
        LOG_TOPIC("6c54f", DEBUG, Logger::COMMUNICATION)
            << "shutdown send stream failed with: " << ec.message();
      }
    }

    if (mustCloseReceive) {
      this->shutdownReceive(ec);
      if (ec && ec != asio_ns::error::not_connected) {
        LOG_TOPIC("215b7", DEBUG, Logger::COMMUNICATION)
            << "shutdown receive stream failed with: " << ec.message();
      }
    }
  }

  void post(std::function<void()>&& handler) {
    _context.post(std::move(handler));
  }

  bool runningInThisThread() { return true; }
  
  uint64_t clients() const {
    return _context._clients.load(std::memory_order_acquire);
  }
  
  rest::GeneralServer::IoContext& context() { return _context; }

 public:
  virtual std::string peerAddress() const = 0;
  virtual int peerPort() const = 0;
  virtual void setNonBlocking(bool) = 0;
  virtual size_t writeSome(basics::StringBuffer* buffer, asio_ns::error_code& ec) = 0;
  virtual void asyncWrite(asio_ns::mutable_buffers_1 const& buffer,
                          AsyncHandler const& handler) = 0;
  virtual size_t readSome(asio_ns::mutable_buffers_1 const& buffer,
                          asio_ns::error_code& ec) = 0;
  virtual std::size_t available(asio_ns::error_code& ec) = 0;
  virtual void asyncRead(asio_ns::mutable_buffers_1 const& buffer,
                         AsyncHandler const& handler) = 0;
  virtual void close(asio_ns::error_code& ec) = 0;

 protected:
  virtual bool sslHandshake() = 0;
  virtual void shutdownReceive(asio_ns::error_code& ec) = 0;
  virtual void shutdownSend(asio_ns::error_code& ec) = 0;

 protected:
  rest::GeneralServer::IoContext& _context;

 private:
  bool const _encrypted;
  bool _handshakeDone = false;
};
}  // namespace arangodb

#endif
