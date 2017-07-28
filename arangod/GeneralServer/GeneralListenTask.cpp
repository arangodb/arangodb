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
#include "GeneralServer/HttpCommTask.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief listen to given port
////////////////////////////////////////////////////////////////////////////////

GeneralListenTask::GeneralListenTask(EventLoop loop, GeneralServer* server,
                                     Endpoint* endpoint,
                                     ProtocolType connectionType)
    : Task(loop, "GeneralListenTask"),
      ListenTask(loop, endpoint),
      _server(server),
      _connectionType(connectionType) {
  _keepAliveTimeout = GeneralServerFeature::keepAliveTimeout();
  
  TRI_ASSERT(_connectionType == ProtocolType::HTTP || _connectionType == ProtocolType::HTTPS);
}

void GeneralListenTask::handleConnected(std::unique_ptr<Socket> socket,
                                        ConnectionInfo&& info) {
  auto commTask = std::make_shared<HttpCommTask>(_loop, _server, std::move(socket),
                                                 std::move(info), _keepAliveTimeout);
  commTask->start();
}
