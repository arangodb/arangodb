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

#ifndef ARANGOD_HTTP_SERVER_HTTP_LISTEN_TASK_H
#define ARANGOD_HTTP_SERVER_HTTP_LISTEN_TASK_H 1

#include "Scheduler/ListenTask.h"

#include <openssl/ssl.h>

#include "GeneralServer/GeneralDefinitions.h"

namespace arangodb {
class Endpoint;

namespace rest {
class GeneralServer;

class GeneralListenTask final : public ListenTask {
  GeneralListenTask(GeneralListenTask const&) = delete;
  GeneralListenTask& operator=(GeneralListenTask const&) = delete;

 public:
  GeneralListenTask(Scheduler*, GeneralServer*, Endpoint*,
                    ProtocolType connectionType);

 protected:
  void handleConnected(std::unique_ptr<Socket>,
                       ConnectionInfo&&) override;

 private:
  GeneralServer* _server;
  ProtocolType const _connectionType;
  double _keepAliveTimeout = 300.0;
};
}
}

#endif
