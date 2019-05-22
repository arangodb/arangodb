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

#include "ListenTask.h"

#include "Basics/MutexLocker.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/Socket.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

ListenTask::ListenTask(GeneralServer& server, 
                       GeneralServer::IoContext& context,
                       Endpoint* endpoint)
    : _server(server),
      _context(context),
      _endpoint(endpoint),
      _acceptFailures(0),
      _bound(false),
      _acceptor(Acceptor::factory(server, context, endpoint)) {}

ListenTask::~ListenTask() {}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

bool ListenTask::start() {
  TRI_ASSERT(_acceptor);

  try {
    _acceptor->open();
  } catch (std::exception const& err) {
    LOG_TOPIC("7c359", WARN, arangodb::Logger::COMMUNICATION)
        << "failed to open endpoint '" << _endpoint->specification()
        << "' with error: " << err.what();
    return false;
  }

  _bound = true;
  this->accept();
  return true;
}

void ListenTask::accept() {
  auto self = shared_from_this();

  auto handler = [this, self](asio_ns::error_code const& ec) {
    TRI_ASSERT(_acceptor != nullptr);

    if (ec) {
      if (ec == asio_ns::error::operation_aborted) {
        // this "error" is accpepted, so it doesn't justify a warning
        LOG_TOPIC("74339", DEBUG, arangodb::Logger::FIXME) << "accept failed: " << ec.message();
        return;
      }

      ++_acceptFailures;

      if (_acceptFailures <= MAX_ACCEPT_ERRORS) {
        LOG_TOPIC("644df", WARN, arangodb::Logger::FIXME) << "accept failed: " << ec.message();
        if (_acceptFailures == MAX_ACCEPT_ERRORS) {
          LOG_TOPIC("40ca3", WARN, arangodb::Logger::FIXME)
              << "too many accept failures, stopping to report";
        }
      }
    }

    std::unique_ptr<Socket> peer = _acceptor->movePeer();

    // set the endpoint
    ConnectionInfo info;
    info.endpoint = _endpoint->specification();
    info.endpointType = _endpoint->domainType();
    info.encryptionType = _endpoint->encryption();
    info.clientAddress = peer->peerAddress();
    info.clientPort = peer->peerPort();
    info.serverAddress = _endpoint->host();
    info.serverPort = _endpoint->port();

    handleConnected(std::move(peer), std::move(info));

    this->accept();
  };

  _acceptor->asyncAccept(handler);
}

void ListenTask::stop() {
  if (!_bound) {
    return;
  }

  _bound = false;
  _acceptor->close();
}
