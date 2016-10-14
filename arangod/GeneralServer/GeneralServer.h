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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_HTTP_SERVER_H
#define ARANGOD_HTTP_SERVER_HTTP_SERVER_H 1

#include "Basics/Common.h"

#include <boost/lockfree/queue.hpp>

#include "Basics/ConditionVariable.h"
#include "Endpoint/ConnectionInfo.h"
#include "GeneralServer/GeneralDefinitions.h"
#include "GeneralServer/HttpCommTask.h"
#include "GeneralServer/RestHandler.h"

namespace arangodb {
class EndpointList;
class ListenTask;

namespace rest {
class GeneralServer {
  GeneralServer(GeneralServer const&) = delete;
  GeneralServer const& operator=(GeneralServer const&) = delete;

 public:
  GeneralServer() = default;
  virtual ~GeneralServer();

 public:
  void setEndpointList(const EndpointList* list);
  void startListening();
  void stopListening();

 protected:
  bool openEndpoint(Endpoint* endpoint);

 protected:
  std::vector<ListenTask*> _listenTasks;
  EndpointList const* _endpointList = nullptr;
};
}
}

#endif
