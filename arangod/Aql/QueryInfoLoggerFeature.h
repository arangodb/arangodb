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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/arangod.h"

#include <velocypack/String.h>

#include <memory>

namespace arangodb::aql {
class QueryInfoLoggerThread;

class QueryInfoLoggerFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "QueryInfoLogger";
  }

  explicit QueryInfoLoggerFeature(Server& server);

  ~QueryInfoLoggerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void beginShutdown() override;
  void start() override;
  void stop() override;

  bool shouldLog(bool isSystem, bool isSlowQuery) const noexcept;

  void log(std::shared_ptr<velocypack::String> const& query);

 private:
  void stopThread();

  std::unique_ptr<QueryInfoLoggerThread> _loggerThread;

  // maximum number of queries to store before they can be logged
  size_t _maxBufferedQueries;

  // if false, no logging will happen at all. if true, consider all other config
  // options.
  bool _logEnabled;

  // if true, log queries in system database, otherwise only non-system.
  bool _logSystemDatabaseQueries;

  // if true, log all slow queries (while honoring _logSystemDatabaseQueries).
  bool _logSlowQueries;

  // probability with which queries are logged. scaled between 0.0 and 100.0.
  double _logProbability;

  // push interval, specified in milliseconds
  uint64_t _pushInterval;

  // collection cleanup interval, specified in milliseconds.
  uint64_t _cleanupInterval;

  // retention time for which queries are kept in the _queries system collection
  // before they are purged. specified in seconds.
  double _retentionTime;
};

}  // namespace arangodb::aql
