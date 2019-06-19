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

#ifndef ARANGOD_GENERAL_SERVER_GENERAL_SERVER_H
#define ARANGOD_GENERAL_SERVER_GENERAL_SERVER_H 1

#include "Basics/Thread.h"
#include "GeneralServer/IoContext.h"

#include <mutex>
#include <set>

namespace arangodb {
class Endpoint;
class EndpointList;

namespace rest {
class Acceptor;
class GeneralCommTask;

class GeneralServer {
  GeneralServer(GeneralServer const&) = delete;
  GeneralServer const& operator=(GeneralServer const&) = delete;

 public:
  explicit GeneralServer(uint64_t numIoThreads);
  ~GeneralServer();

 public:
  void registerTask(std::unique_ptr<rest::GeneralCommTask>);
  void unregisterTask(rest::GeneralCommTask*);
  void setEndpointList(EndpointList const* list);
  void startListening();
  void stopListening();
  void stopWorking();

  IoContext& selectIoContext();
  asio_ns::ssl::context& sslContext();

 protected:
  bool openEndpoint(IoContext& ioContext, Endpoint* endpoint);

 private:
  EndpointList const* _endpointList;
  std::vector<IoContext> _contexts;

  std::recursive_mutex _tasksLock;
  std::vector<std::unique_ptr<Acceptor>> _acceptors;
  std::map<void*, std::unique_ptr<rest::GeneralCommTask>> _commTasks;

  /// protect ssl context creation
  std::mutex _sslContextMutex;
  /// global SSL context to use here
  std::unique_ptr<asio_ns::ssl::context> _sslContext;
};
}  // namespace rest
}  // namespace arangodb

#endif
