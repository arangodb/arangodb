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

#include "GeneralListenTask.h"

#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/VppCommTask.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Ssl/SslServerFeature.h"

using namespace arangodb;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief listen to given port
////////////////////////////////////////////////////////////////////////////////

GeneralListenTask::GeneralListenTask(GeneralServer* server, Endpoint* endpoint,
                                     ProtocolType connectionType)
    : Task("GeneralListenTask"),
      ListenTask(endpoint),
      _server(server),
      _connectionType(connectionType) {
  _keepAliveTimeout = GeneralServerFeature::keepAliveTimeout();

  SslServerFeature* ssl =
      application_features::ApplicationServer::getFeature<SslServerFeature>(
          "SslServer");

  if (ssl != nullptr) {
    _sslContext = ssl->sslContext();
  }

  _verificationMode = GeneralServerFeature::verificationMode();
  _verificationCallback = GeneralServerFeature::verificationCallback();
}

bool GeneralListenTask::handleConnected(TRI_socket_t socket,
                                        ConnectionInfo&& info) {
  GeneralCommTask* commTask = nullptr;

  switch (_connectionType) {
    case ProtocolType::VPPS:
      commTask =
          new VppCommTask(_server, socket, std::move(info), _keepAliveTimeout);
      break;
    case ProtocolType::VPP:
      commTask =
          new VppCommTask(_server, socket, std::move(info), _keepAliveTimeout);
      break;
    case ProtocolType::HTTPS:
      commTask = new HttpsCommTask(_server, socket, std::move(info),
                                   _keepAliveTimeout, _sslContext,
                                   _verificationMode, _verificationCallback);
      break;
    case ProtocolType::HTTP:
      commTask =
          new HttpCommTask(_server, socket, std::move(info), _keepAliveTimeout);
      break;
    default:
      return false;
  }

  SchedulerFeature::SCHEDULER->registerTask(commTask);
  return true;
}
