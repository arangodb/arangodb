//////////////////////////////////////////////////////////////////////////////
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "GeneralCommTask.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/compile-time-strlen.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/Locking.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "Meta/conversion.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"
#include "Utils/Events.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
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
  _protocol->setNonBlocking(true);
  this->asyncReadSome();
}

template <SocketType T>
void GeneralCommTask<T>::close() {
  LOG_DEVEL << "HttpCommTask::close(" << this << ")";
  if (_protocol) {
    _protocol->timer.cancel();
    asio_ns::error_code ec;
    _protocol->shutdown(ec);
    if (ec) {
      LOG_DEVEL << ec.message();
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
    while (!ec && available > 16) {
      auto mutableBuff = _readBuffer.prepare(available);
      size_t nread = _protocol->socket.read_some(mutableBuff, ec);
      _readBuffer.commit(nread);
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
  if (_readBuffer.size() > 0 && !readCallback(ec)) {
    LOG_DEVEL << "reading pipelined request";
    return;
  }
  
  auto cb = [self = shared_from_this()](asio_ns::error_code const& ec,
                                        size_t transferred) {
    auto* thisPtr = static_cast<GeneralCommTask<T>*>(self.get());
    thisPtr->_readBuffer.commit(transferred);
    if (thisPtr->readCallback(ec)) {
      thisPtr->asyncReadSome();
    }
  };
  auto mutableBuff = _readBuffer.prepare(READ_BLOCK_SIZE);
  _protocol->socket.async_read_some(mutableBuff, std::move(cb));
}
