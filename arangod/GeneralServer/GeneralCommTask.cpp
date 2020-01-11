//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

template <SocketType T>
GeneralCommTask<T>::GeneralCommTask(GeneralServer& server,
                                    ConnectionInfo info,
                                    std::unique_ptr<AsioSocket<T>> socket)
    : CommTask(server, std::move(info)), _protocol(std::move(socket)) {}

template <SocketType T>
void GeneralCommTask<T>::start() {
  asio_ns::post(_protocol->context.io_context, [self = shared_from_this()] {
    auto* me = static_cast<GeneralCommTask<T>*>(self.get());
    if (AsioSocket<T>::supportsMixedIO()) {
      me->_protocol->setNonBlocking(true);
    }
    me->asyncReadSome();
  });
}

template <SocketType T>
void GeneralCommTask<T>::stop() {
  asio_ns::post(_protocol->context.io_context, [self = shared_from_this()] {
    static_cast<GeneralCommTask<T>&>(*self).close(asio_ns::error_code());
  });
}

template <SocketType T>
void GeneralCommTask<T>::close(asio_ns::error_code const& err) {
  if (err && err != asio_ns::error::misc_errors::eof) {
    LOG_TOPIC("2b6b3", WARN, arangodb::Logger::REQUESTS)
    << "asio IO error: '" << err.message() << "'";
  }
  
  if (_protocol) {
    _protocol->timer.cancel();
    asio_ns::error_code ec;
    _protocol->shutdown(ec);
    if (ec) {
      LOG_TOPIC("2c6b4", ERR, arangodb::Logger::REQUESTS)
          << "error shutting down asio socket: '" << ec.message() << "'";
    }
  }
  _server.unregisterTask(this);  // will delete us
}

/// set / reset connection timeout
template <SocketType T>
void GeneralCommTask<T>::setTimeout(std::chrono::milliseconds millis) {
  this->_protocol->timer.expires_after(millis);
  this->_protocol->timer.async_wait([self = CommTask::weak_from_this()](asio_ns::error_code ec) {
    std::shared_ptr<CommTask> s;
    if (ec || !(s = self.lock())) {  // was canceled / deallocated
      return;
    }
    LOG_TOPIC("5c1e0", INFO, Logger::REQUESTS)
        << "keep alive timeout, closing stream!";
    static_cast<GeneralCommTask<T>&>(*s).close(ec);
  });
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
  
  // VST and H2 get a simple timeout here
  if (enableReadTimeout()) {
    setTimeout(DefaultTimeout);
  }

  auto mutableBuff = _protocol->buffer.prepare(ReadBlockSize);
  _protocol->socket.async_read_some(
      mutableBuff, [self = shared_from_this()](asio_ns::error_code const& ec, size_t nread) {
        auto& me = static_cast<GeneralCommTask<T>&>(*self);
        me._protocol->buffer.commit(nread);
  
        if (me.enableReadTimeout()) {
          me._protocol->timer.cancel();
        }

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
