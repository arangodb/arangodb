////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Basics/Result.h"
#include "Basics/Thread.h"
#include "GeneralServer/IoContext.h"
#include "GeneralServer/SslServerFeature.h"

#include <mutex>
#include <map>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class Endpoint;
class EndpointList;
class GeneralServerFeature;

namespace rest {
class Acceptor;
class CommTask;

class GeneralServer {
  GeneralServer(GeneralServer const&) = delete;
  GeneralServer const& operator=(GeneralServer const&) = delete;

 public:
  explicit GeneralServer(GeneralServerFeature&, uint64_t numIoThreads,
                         bool allowEarlyConnections);
  ~GeneralServer();

  void registerTask(std::shared_ptr<rest::CommTask>);
  void unregisterTask(rest::CommTask*);
  void startListening(EndpointList& list);  /// start accepting connections
  void stopListening();                     /// stop accepting new connections
  void stopConnections();                   /// stop connections
  void stopWorking();

  bool allowEarlyConnections() const noexcept;
  IoContext& selectIoContext();
  SslServerFeature::SslContextList sslContexts();
  SSL_CTX* getSSL_CTX(size_t index);

  ArangodServer& server() const;

  Result reloadTLS();

 protected:
  bool openEndpoint(IoContext& ioContext, Endpoint* endpoint);

 private:
  GeneralServerFeature& _feature;
  std::vector<IoContext> _contexts;
  bool const _allowEarlyConnections;

  std::recursive_mutex _tasksLock;
  std::vector<std::unique_ptr<Acceptor>> _acceptors;
  std::map<void*, std::shared_ptr<rest::CommTask>> _commTasks;

  /// protect ssl context creation
  std::mutex _sslContextMutex;
  /// global SSL context to use here
  SslServerFeature::SslContextList _sslContexts;
};
}  // namespace rest
}  // namespace arangodb
