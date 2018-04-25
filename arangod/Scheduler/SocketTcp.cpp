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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "Scheduler/SocketTcp.h"
#include "Logger/Logger.h"

using namespace arangodb;

namespace  {
  template <typename T>
  bool doSslHandshake(T& socket) {
    boost::system::error_code ec;
    
    uint64_t tries = 0;
    double start = 0.0;
    
    while (true) {
      ec.assign(boost::system::errc::success,
                boost::system::generic_category());
      socket.handshake(boost::asio::ssl::stream_base::handshake_type::server, ec);
      
      if (ec.value() != boost::asio::error::would_block) {
        break;
      }
      
      // got error EWOULDBLOCK and need to try again
      ++tries;
      
      // following is a helpless fix for connections hanging in the handshake
      // phase forever. we've seen this happening when the underlying peer
      // connection was closed during the handshake.
      // with the helpless fix, handshakes will be aborted it they take longer
      // than x seconds. a proper fix is to make the handshake run asynchronously
      // and somehow signal it that the connection got closed. apart from that
      // running it asynchronously will not block the scheduler thread as it
      // does now. anyway, even the helpless fix allows self-healing of busy
      // scheduler threads after a network failure
      if (tries == 1) {
        // capture start time of handshake
        start = TRI_microtime();
      } else if (tries % 50 == 0) {
        // check if we have spent more than x seconds handshaking and then abort
        TRI_ASSERT(start != 0.0);
        
        if (TRI_microtime() - start >= 3) {
          ec.assign(boost::asio::error::connection_reset,
                    boost::system::generic_category());
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "forcefully shutting down connection after wait time";
          break;
        } else {
          std::this_thread::sleep_for(std::chrono::microseconds(10000));
        }
      }
      
      // next iteration
    }
    
    if (ec) {
      // this message will also be emitted if a connection is attempted
      // with a wrong protocol (e.g. HTTP instead of SSL/TLS). so it's
      // definitely not worth logging an error here
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << "unable to perform ssl handshake: " << ec.message() << " : "
      << ec.value();
      return false;
    }
    return true;
  }
}

bool SocketSslTcp::sslHandshake() {
  //MUTEX_LOCKER(guard, _lock);
  return ::doSslHandshake(_sslSocket);
}
/*
size_t SocketTcp::write(basics::StringBuffer* buffer,
                        boost::system::error_code& ec) {
  //MUTEX_LOCKER(guard, _lock);
  if (_encrypted) {
    return _sslSocket.write_some(boost::asio::buffer(buffer->begin(), buffer->length()), ec);
  } else {
    return _socket.write_some(boost::asio::buffer(buffer->begin(), buffer->length()), ec);
  }
}

void SocketTcp::asyncWrite(boost::asio::mutable_buffers_1 const& buffer,
                           AsyncHandler const& handler) {
  //MUTEX_LOCKER(guard, _lock);
  if (_encrypted) {
    return boost::asio::async_write(_sslSocket, buffer, handler);
  } else {
    return boost::asio::async_write(_socket, buffer, handler);
  }
}

size_t SocketTcp::read(boost::asio::mutable_buffers_1 const& buffer,
                       boost::system::error_code& ec) {
  //MUTEX_LOCKER(guard, _lock);
  if (_encrypted) {
    return _sslSocket.read_some(buffer, ec);
  } else {
    return _socket.read_some(buffer, ec);
  }
}

void SocketTcp::shutdown(boost::system::error_code& ec, bool closeSend, bool closeReceive) {
  MUTEX_LOCKER(guard, _lock);
  Socket::shutdown(ec, closeSend, closeReceive);
}
 void SocketTcp::asyncRead(boost::asio::mutable_buffers_1 const& buffer,
 AsyncHandler const& handler) {
 //MUTEX_LOCKER(guard, _lock);
 if (_encrypted) {
 return _sslSocket.async_read_some(buffer, handler);
 } else {
 return _socket.async_read_some(buffer, handler);
 }
 }*/

void SocketTcp::close(boost::system::error_code& ec) {
  //MUTEX_LOCKER(guard, _lock);
  if (_socket.is_open()) {
    _socket.close(ec);
    if (ec && ec != boost::asio::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "closing socket failed with: " << ec.message();
    }
  }
}

/*std::size_t SocketTcp::available(boost::system::error_code& ec) {
  //MUTEX_LOCKER(guard, _lock);
  return static_cast<size_t>(_socket.available(ec));
}*/

void SocketTcp::shutdownReceive(boost::system::error_code& ec) {
  _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
}

void SocketTcp::shutdownSend(boost::system::error_code& ec) {
  _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}

void SocketSslTcp::close(boost::system::error_code& ec) {
  //MUTEX_LOCKER(guard, _lock);
  if (_socket.is_open()) {
    _socket.close(ec);
    if (ec && ec != boost::asio::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << "closing socket failed with: " << ec.message();
    }
  }
}

/*std::size_t SocketTcp::available(boost::system::error_code& ec) {
 //MUTEX_LOCKER(guard, _lock);
 return static_cast<size_t>(_socket.available(ec));
 }*/

void SocketSslTcp::shutdownReceive(boost::system::error_code& ec) {
  _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
}

void SocketSslTcp::shutdownSend(boost::system::error_code& ec) {
  _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}

