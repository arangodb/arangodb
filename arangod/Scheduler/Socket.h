////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/serial_port_service.hpp>
#include <boost/asio/ssl.hpp>

#include "Basics/StringBuffer.h"
#include "Basics/asio-helper.h"
#include "Logger/Logger.h"

namespace arangodb {

typedef std::function<void(const boost::system::error_code& ec,
                           std::size_t transferred)>
    AsyncHandler;

namespace socketcommon {
template <typename T>
bool doSslHandshake(T& socket) {
  boost::system::error_code ec;

  do {
    ec.assign(boost::system::errc::success, boost::system::generic_category());
    socket.handshake(boost::asio::ssl::stream_base::handshake_type::server, ec);
  } while (ec.value() == boost::asio::error::would_block);

  if (ec) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION)
        << "unable to perform ssl handshake: " << ec.message() << " : "
        << ec.value();
    return false;
  }
  return true;
}

template <typename T>
size_t doWrite(T& socket, basics::StringBuffer* buffer,
               boost::system::error_code& ec) {
  return socket.write_some(
      boost::asio::buffer(buffer->begin(), buffer->length()), ec);
}
template <typename T>
void doAsyncWrite(T& socket, boost::asio::mutable_buffers_1 const& buffer,
                  AsyncHandler const& handler) {
  return boost::asio::async_write(socket, buffer, handler);
}
template <typename T>
size_t doRead(T& socket, boost::asio::mutable_buffers_1 const& buffer,
              boost::system::error_code& ec) {
  return socket.read_some(buffer, ec);
}
template <typename T>
void doAsyncRead(T& socket, boost::asio::mutable_buffers_1 const& buffer,
                 AsyncHandler const& handler) {
  return socket.async_read_some(buffer, handler);
}
}

class Socket {
 public:
  Socket(boost::asio::io_service& ioService,
         boost::asio::ssl::context&& context, bool encrypted)
      : _ioService(ioService),
        _context(std::move(context)),
        _encrypted(encrypted) {}
  Socket(Socket&& that) = default;
  virtual ~Socket() {}

  virtual void close() = 0;
  virtual void close(boost::system::error_code& ec) = 0;
  virtual void setNonBlocking(bool) = 0;
  virtual std::string peerAddress() = 0;
  virtual int peerPort() = 0;
  bool handshake();
  virtual size_t write(basics::StringBuffer* buffer,
                       boost::system::error_code& ec) = 0;
  virtual void asyncWrite(boost::asio::mutable_buffers_1 const& buffer,
                          AsyncHandler const& handler) = 0;
  virtual size_t read(boost::asio::mutable_buffers_1 const& buffer,
                      boost::system::error_code& ec) = 0;
  virtual void shutdownReceive() = 0;
  virtual void shutdownReceive(boost::system::error_code& ec) = 0;
  virtual void shutdownSend(boost::system::error_code& ec) = 0;
  virtual int available(boost::system::error_code& ec) = 0;
  virtual void asyncRead(boost::asio::mutable_buffers_1 const& buffer,
                         AsyncHandler const& handler) = 0;

 protected:
  virtual bool sslHandshake() = 0;

 public:
  boost::asio::io_service& _ioService;
  boost::asio::ssl::context _context;

  bool _encrypted;
  bool _handshakeDone = false;
};
}

#endif
