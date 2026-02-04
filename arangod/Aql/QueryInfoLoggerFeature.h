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
#include "Aql/QueryInfoLoggerOptions.h"

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
  QueryInfoLoggerOptions _options;
};

}  // namespace arangodb::aql
