////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <fuerte/message.h>

#include <mutex>

#include "Aql/AqlExecuteResult.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/RegisterInfos.h"

namespace arangodb::fuerte {
inline namespace v1 {
enum class RestVerb;
}
}  // namespace arangodb::fuerte

namespace arangodb::aql {

class SkipResult;

// The RemoteBlock is actually implemented by specializing ExecutionBlockImpl,
// so this class only exists to identify the specialization.
class RemoteExecutor final {};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template<>
class ExecutionBlockImpl<RemoteExecutor> : public ExecutionBlock {
 public:
  // TODO Even if it's not strictly necessary here, for consistency's sake the
  // non-standard arguments (server, ownName and queryId) should probably be
  // moved into some RemoteExecutorInfos class.
  ExecutionBlockImpl(ExecutionEngine* engine, RemoteNode const* node,
                     RegisterInfos&& infos, std::string const& server,
                     std::string const& distributeId,
                     std::string const& queryId);

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, Result> initializeCursor(
      InputAqlItemRow const& input) override;

  std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> execute(
      AqlCallStack const& stack) override;

  std::string const& distributeId() const { return _distributeId; }

  std::string const& server() const { return _server; }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only for asserts:
 public:
  std::string const& queryId() const { return _queryId; }
#endif

 private:
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSomeWithoutTrace(
      size_t atMost);

  std::pair<ExecutionState, size_t> skipSomeWithoutTrace(size_t atMost);

  auto executeWithoutTrace(AqlCallStack const& stack)
      -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>;

  [[nodiscard]] auto deserializeExecuteCallResultBody(velocypack::Slice) const
      -> ResultT<AqlExecuteResult>;
  [[nodiscard]] auto serializeExecuteCallBody(
      AqlCallStack const& callStack) const -> velocypack::Buffer<uint8_t>;

  RegisterInfos const& registerInfos() const { return _registerInfos; }

  QueryContext const& getQuery() const { return _query; }

  /// @brief internal method to send a request. Will register a callback to be
  /// reactivated
  arangodb::Result sendAsyncRequest(fuerte::RestVerb type,
                                    std::string const& urlPart,
                                    velocypack::Buffer<uint8_t>&& body);

  // _communicationMutex *must* be locked for this!
  unsigned generateRequestTicket();

  void traceExecuteRequest(velocypack::Slice slice,
                           AqlCallStack const& callStack);
  void traceGetSomeRequest(velocypack::Slice slice, size_t atMost);
  void traceSkipSomeRequest(velocypack::Slice slice, size_t atMost);
  void traceInitializeCursorRequest(velocypack::Slice slice);
  void traceRequest(char const* rpc, velocypack::Slice slice,
                    std::string const& args);

 private:
  RegisterInfos _registerInfos;

  QueryContext const& _query;

  /// @brief our server, can be like "shard:S1000" or like "server:Claus"
  std::string const _server;

  /// @brief our own identity, in case of the coordinator this is empty,
  /// in case of the DBservers, this is the shard ID as a string
  std::string const _distributeId;

  /// @brief the ID of the query on the server as a string
  std::string const _queryId;

  std::mutex _communicationMutex;

  /// @brief the last unprocessed result. Make sure to reset it
  ///        after it is processed.
  std::unique_ptr<fuerte::Response> _lastResponse;

  /// @brief the last remote response Result object, may contain an error.
  arangodb::Result _lastError;

  /// @brief whether or not this block will forward initialize,
  /// initializeCursor requests
  bool const _isResponsibleForInitializeCursor;

  bool _requestInFlight;

  unsigned _lastTicket;  /// used to check for canceled requests
};

}  // namespace arangodb::aql
