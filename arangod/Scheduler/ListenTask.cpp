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

#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Scheduler/Acceptor.h"
#include "Ssl/SslServerFeature.h"

using namespace arangodb;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

ListenTask::ListenTask(EventLoop loop, Endpoint* endpoint)
    : Task(loop, "ListenTask"),
      _endpoint(endpoint),
      _bound(false),
      _ioService(loop._ioService),
      _acceptor(Acceptor::factory(*loop._ioService, endpoint)) {}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void ListenTask::start() {
  try {
    _acceptor->open();
    _bound = true;
  } catch (boost::system::system_error const& err) {
    LOG(WARN) << "failed to open endpoint '" << _endpoint->specification()
              << "' with error: " << err.what();
    return;
  }

  _handler = [this](boost::system::error_code const& ec) {
    if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
        return;
      }

      ++_acceptFailures;

      if (_acceptFailures < MAX_ACCEPT_ERRORS) {
        LOG(WARN) << "accept failed: " << ec.message();
      } else if (_acceptFailures == MAX_ACCEPT_ERRORS) {
        LOG(WARN) << "accept failed: " << ec.message();
        LOG(WARN) << "too many accept failures, stopping to report";
      }
    }

    ConnectionInfo info;

    auto peer = _acceptor->movePeer();

    // set the endpoint
    info.endpoint = _endpoint->specification();
    info.endpointType = _endpoint->domainType();
    info.encryptionType = _endpoint->encryption();
    info.clientAddress = peer->peerAddress();
    info.clientPort = peer->peerPort();
    info.serverAddress = _endpoint->host();
    info.serverPort = _endpoint->port();

    handleConnected(std::move(peer), std::move(info));

    if (_bound) {
      _acceptor->asyncAccept(_handler);
    }
  };

  _acceptor->asyncAccept(_handler);
}

void ListenTask::stop() {
  if (!_bound) {
    return;
  }

  _bound = false;
  _acceptor->close();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void ListenTask::createPeer() {
  if (_endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    _peer.reset(new Socket(*_ioService,
                           SslServerFeature::SSL->createSslContext(), true));
  } else {
    _peer.reset(new Socket(
        *_ioService,
        boost::asio::ssl::context(boost::asio::ssl::context::method::sslv23),
        false));
  }
>>>>>>> 72280a99cc967ac09d556fe04b817e4dd01a1b0d
}
