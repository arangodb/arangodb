////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb {

template<typename T>
class ResultT;

class Result;

namespace futures {
template<typename T>
class Future;
}
}  // namespace arangodb

namespace arangodb::replication2 {
class LogId;

/**
 * This is a collection of functions that is used by the RestHandler. It covers
 * two different implementations. One for dbservers that actually execute the
 * commands and one for coordinators that forward the request to the leader.
 */
struct PrototypeStateMethods {
  virtual ~PrototypeStateMethods() = default;

  struct CreateOptions {
    bool waitForReady{false};
    std::optional<LogId> id;
    std::optional<LogConfig> config;
    std::vector<ParticipantId> servers;
  };

  struct CreateResult {
    LogId id;
    std::vector<ParticipantId> servers;
  };

  [[nodiscard]] virtual auto createState(CreateOptions options) const
      -> futures::Future<ResultT<CreateResult>> = 0;

  [[nodiscard]] virtual auto insert(
      LogId id,
      std::unordered_map<std::string, std::string> const& entries) const
      -> futures::Future<ResultT<LogIndex>> = 0;

  [[nodiscard]] virtual auto get(LogId id, std::string key) const
      -> futures::Future<std::optional<std::string>> = 0;
  [[nodiscard]] virtual auto get(LogId id, std::vector<std::string> keys) const
      -> futures::Future<std::unordered_map<std::string, std::string>> = 0;

  [[nodiscard]] virtual auto getSnapshot(LogId id, LogIndex waitForIndex) const
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> = 0;

  [[nodiscard]] virtual auto remove(LogId id, std::string key) const
      -> futures::Future<ResultT<LogIndex>> = 0;
  [[nodiscard]] virtual auto remove(LogId id,
                                    std::vector<std::string> keys) const
      -> futures::Future<ResultT<LogIndex>> = 0;

  struct PrototypeStatus {
    // TODO
    LogId id;
  };

  [[nodiscard]] virtual auto status(LogId) const
      -> futures::Future<ResultT<PrototypeStatus>> = 0;

  static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<PrototypeStateMethods>;
};

template<class Inspector>
auto inspect(Inspector& f, PrototypeStateMethods::CreateOptions& x) {
  return f.object(x).fields(
      f.field("waitForReady", x.waitForReady).fallback(true),
      f.field("id", x.id), f.field("config", x.config),
      f.field("servers", x.servers).fallback(std::vector<ParticipantId>{}));
}
template<class Inspector>
auto inspect(Inspector& f, PrototypeStateMethods::CreateResult& x) {
  return f.object(x).fields(f.field("id", x.id), f.field("servers", x.servers));
}
template<class Inspector>
auto inspect(Inspector& f, PrototypeStateMethods::PrototypeStatus& x) {
  return f.object(x).fields(f.field("id", x.id));
}

}  // namespace arangodb::replication2
