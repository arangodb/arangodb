////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_REMOTE_EXECUTOR_H
#define ARANGOD_AQL_REMOTE_EXECUTOR_H

#include "Aql/ClusterNodes.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/ExecutionBlockImpl.h"

#include <mutex>

#include <fuerte/message.h>
#include <fuerte/types.h>

namespace arangodb {
namespace aql {

// The RemoteBlock is actually implemented by specializing ExecutionBlockImpl,
// so this class only exists to identify the specialization.
class RemoteExecutor final {};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template <>
class ExecutionBlockImpl<RemoteExecutor> : public ExecutionBlock {
 public:
  // TODO Even if it's not strictly necessary here, for consistency's sake the
  // non-standard arguments (server, ownName and queryId) should probably be
  // moved into some RemoteExecutorInfos class.
  ExecutionBlockImpl(ExecutionEngine* engine, RemoteNode const* node,
                     ExecutorInfos&& infos, std::string const& server,
                     std::string const& ownName, std::string const& queryId);

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) override;

  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override;

  std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) override;

  std::pair<ExecutionState, Result> shutdown(int errorCode) override;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only for asserts:
 public:
  std::string const& server() const { return _server; }
  std::string const& ownName() const { return _ownName; }
  std::string const& queryId() const { return _queryId; }
#endif

 private:
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSomeWithoutTrace(size_t atMost);

  std::pair<ExecutionState, size_t> skipSomeWithoutTrace(size_t atMost);

  ExecutorInfos const& infos() const { return _infos; }

  Query const& getQuery() const { return _query; }

  /// @brief internal method to send a request. Will register a callback to be
  /// reactivated
  arangodb::Result sendAsyncRequest(fuerte::RestVerb type, std::string const& urlPart,
                                    velocypack::Buffer<uint8_t> body);

 private:
  ExecutorInfos _infos;

  Query const& _query;

  /// @brief our server, can be like "shard:S1000" or like "server:Claus"
  std::string const _server;

  /// @brief our own identity, in case of the coordinator this is empty,
  /// in case of the DBservers, this is the shard ID as a string
  std::string const _ownName;

  /// @brief the ID of the query on the server as a string
  std::string const _queryId;

  /// @brief whether or not this block will forward initialize,
  /// initializeCursor or shutDown requests
  bool const _isResponsibleForInitializeCursor;

  /// @brief the last unprocessed result. Make sure to reset it
  ///        after it is processed.
  std::unique_ptr<fuerte::Response> _lastResponse;

  /// @brief the last remote response Result object, may contain an error.
  arangodb::Result _lastError;
  
  std::mutex _communicationMutex;
  
  unsigned _lastTicket;  /// used to check for canceled requests
  
  bool _hasTriggeredShutdown;

  // _communicationMutex *must* be locked for this!
  unsigned generateNewTicket();
  
  bool _didSendShutdownRequest = false;

  void traceGetSomeRequest(velocypack::Slice slice, size_t atMost);
  void traceSkipSomeRequest(velocypack::Slice slice, size_t atMost);
  void traceShutdownRequest(velocypack::Slice slice, int errorCode);
  void traceRequest(const char* rpc, velocypack::Slice slice, std::string const& args);
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_REMOTE_EXECUTOR_H
