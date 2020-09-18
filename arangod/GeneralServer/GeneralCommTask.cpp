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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "GeneralCommTask.h"

#include "GeneralServer/GeneralServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::rest;

template <SocketType T>
GeneralCommTask<T>::GeneralCommTask(GeneralServer& server,
                                    ConnectionInfo info,
                                    std::unique_ptr<AsioSocket<T>> socket)
: CommTask(server, std::move(info)),
  _protocol(std::move(socket)),
  _reading(false),
  _writing(false),
  _stopped(false) {
  if (AsioSocket<T>::supportsMixedIO()) {
    _protocol->setNonBlocking(true);
  }
}

template <SocketType T>
void GeneralCommTask<T>::stop() {
  _stopped.store(true, std::memory_order_release);
  if (!_protocol) {
    return;
  }
  asio_ns::dispatch(_protocol->context.io_context, [self = shared_from_this()] {
    static_cast<GeneralCommTask<T>&>(*self).close(asio_ns::error_code());
  });
}

template <SocketType T>
void GeneralCommTask<T>::close(asio_ns::error_code const& ec) {
  _stopped.store(true, std::memory_order_release);
  if (ec && ec != asio_ns::error::misc_errors::eof) {
    LOG_TOPIC("2b6b3", WARN, arangodb::Logger::REQUESTS)
    << "asio IO error: '" << ec.message() << "'";
  }
  
  if (_protocol) {
    _protocol->timer.cancel();
    _protocol->shutdown([this, self(shared_from_this())](asio_ns::error_code ec) {
      if (ec) {
        LOG_TOPIC("2c6b4", INFO, arangodb::Logger::REQUESTS)
            << "error shutting down asio socket: '" << ec.message() << "'";
      }
      _server.unregisterTask(this);
    });
  } else {
    _server.unregisterTask(this);  // will delete us
  }
}

template <SocketType T>
void GeneralCommTask<T>::asyncReadSome() try {
  asio_ns::error_code ec;
  // first try a sync read for performance
  if (AsioSocket<T>::supportsMixedIO()) {
    std::size_t available = _protocol->available(ec);

    while (!ec && available > 8) {
      auto mutableBuff = _protocol->buffer.prepare(available);
      size_t nread = _protocol->socket.read_some(mutableBuff, ec);
      _protocol->buffer.commit(nread);
      if (ec) {
        break;
      }

      if (!readCallback(ec)) {
        return;
      }
      available = _protocol->available(ec);
    }
    if (ec == asio_ns::error::would_block) {
      ec.clear();
    }
  }

  // read pipelined requests / remaining data
  if (_protocol->buffer.size() > 0 && !readCallback(ec)) {
    return;
  }
  
  auto mutableBuff = _protocol->buffer.prepare(ReadBlockSize);
  
  _reading = true;
  setIOTimeout();
  _protocol->socket.async_read_some(
      mutableBuff, [self = shared_from_this()](asio_ns::error_code const& ec, size_t nread) {
        auto& me = static_cast<GeneralCommTask<T>&>(*self);
        me._reading = false;
        me._protocol->buffer.commit(nread);

        try {
          if (me.readCallback(ec)) {
            me.asyncReadSome();
          }
        } catch (...) {
          LOG_TOPIC("2c6b6", ERR, arangodb::Logger::REQUESTS)
              << "unhandled protocol exception, closing connection";
          me.close(ec);
        }
      });
} catch (...) {
  LOG_TOPIC("2c6b5", ERR, arangodb::Logger::REQUESTS)
      << "unhandled protocol exception, closing connection";
  close();
}

template class arangodb::rest::GeneralCommTask<SocketType::Tcp>;
template class arangodb::rest::GeneralCommTask<SocketType::Ssl>;
#ifndef _WIN32
template class arangodb::rest::GeneralCommTask<SocketType::Unix>;
#endif
