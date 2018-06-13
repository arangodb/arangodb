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
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Scheduler/Acceptor.h"
#include "Scheduler/JobGuard.h"

using namespace arangodb;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

ListenTask::ListenTask(Scheduler* scheduler, Endpoint* endpoint)
    : Task(scheduler, "ListenTask"),
      _endpoint(endpoint),
      _bound(false),
      _acceptor(Acceptor::factory(scheduler, endpoint)) {}

ListenTask::~ListenTask() {}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

bool ListenTask::start() {
  MUTEX_LOCKER(mutex, _shutdownMutex);
  TRI_ASSERT(_acceptor);

  try {
    _acceptor->open();
  } catch (asio_ns::system_error const& err) {
    LOG_TOPIC(WARN, arangodb::Logger::COMMUNICATION)
        << "failed to open endpoint '" << _endpoint->specification()
        << "' with error: " << err.what();
    return false;
  } catch (std::exception const& err) {
    LOG_TOPIC(WARN, arangodb::Logger::COMMUNICATION)
        << "failed to open endpoint '" << _endpoint->specification()
        << "' with error: " << err.what();
    return false;
  }

  _handler = [this](asio_ns::error_code const& ec) {
    MUTEX_LOCKER(mutex, _shutdownMutex);
    JobGuard guard(_scheduler);
    guard.work();

    if (!_bound) {
      _handler = nullptr;
      return;
    }

    TRI_ASSERT(_handler != nullptr);
    TRI_ASSERT(_acceptor != nullptr);

    if (ec) {
      if (ec == asio_ns::error::operation_aborted) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "accept failed: "
                                                 << ec.message();
        return;
      }

      ++_acceptFailures;

      if (_acceptFailures < MAX_ACCEPT_ERRORS) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "accept failed: "
                                                 << ec.message();
      } else if (_acceptFailures == MAX_ACCEPT_ERRORS) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "accept failed: "
                                                 << ec.message();
        LOG_TOPIC(WARN, arangodb::Logger::FIXME)
            << "too many accept failures, stopping to report";
      }
    }

    ConnectionInfo info;

    std::unique_ptr<Socket> peer = _acceptor->movePeer();

    // set the endpoint
    info.endpoint = _endpoint->specification();
    info.endpointType = _endpoint->domainType();
    info.encryptionType = _endpoint->encryption();
    info.clientAddress = peer->peerAddress();
    info.clientPort = peer->peerPort();
    info.serverAddress = _endpoint->host();
    info.serverPort = _endpoint->port();

    handleConnected(std::move(peer), std::move(info));

    _acceptor->asyncAccept(_handler);
  };

  _bound = true;
  _acceptor->asyncAccept(_handler);
  return true;
}

void ListenTask::stop() {
  MUTEX_LOCKER(mutex, _shutdownMutex);

  if (!_bound) {
    return;
  }

  _bound = false;
  _handler = nullptr;

  _acceptor->close();
  _acceptor.reset();
}
