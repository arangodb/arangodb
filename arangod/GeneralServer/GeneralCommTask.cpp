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
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

template <SocketType T>
GeneralCommTask<T>::GeneralCommTask(GeneralServer& server,
                                 char const* name,
                                 ConnectionInfo info,
                                 std::unique_ptr<AsioSocket<T>> socket)
  : CommTask(server, name, info), _protocol(std::move(socket)) {}

template <SocketType T>
GeneralCommTask<T>::~GeneralCommTask() {
}

template <SocketType T>
void GeneralCommTask<T>::start() {
  asio_ns::post(_protocol->context.io_context, [self = shared_from_this()] {
    auto* thisPtr = static_cast<GeneralCommTask<T>*>(self.get());
    thisPtr->_protocol->setNonBlocking(true);
    thisPtr->asyncReadSome();
  });
}

template <SocketType T>
void GeneralCommTask<T>::close() {
  if (_protocol) {
    _protocol->timer.cancel();
    asio_ns::error_code ec;
    _protocol->shutdown(ec);
    if (ec) {
      LOG_TOPIC("2c6b4", DEBUG, arangodb::Logger::REQUESTS)
      << "error shutting down asio socket: '" << ec.message() << "'";
    }
  }
  _server.unregisterTask(this);  // will delete us
}

template <SocketType T>
void GeneralCommTask<T>::asyncReadSome() {

  asio_ns::error_code ec;
  // first try a sync read for performance
  if (_protocol->supportsMixedIO()) {
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
  
  auto cb = [self = shared_from_this()](asio_ns::error_code const& ec,
                                        size_t transferred) {

    auto* thisPtr = static_cast<GeneralCommTask<T>*>(self.get());
    thisPtr->_protocol->buffer.commit(transferred);
    if (thisPtr->readCallback(ec)) {
      thisPtr->asyncReadSome();
    }
  };
  auto mutableBuff = _protocol->buffer.prepare(ReadBlockSize);
  _protocol->socket.async_read_some(mutableBuff, std::move(cb));
}

template class arangodb::rest::GeneralCommTask<SocketType::Tcp>;
template class arangodb::rest::GeneralCommTask<SocketType::Ssl>;
#ifndef _WIN32
template class arangodb::rest::GeneralCommTask<SocketType::Unix>;
#endif

