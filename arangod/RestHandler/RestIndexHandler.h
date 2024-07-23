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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "Basics/Result.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <velocypack/Builder.h>

namespace arangodb {
class LogicalCollection;

class RestIndexHandler : public arangodb::RestVocbaseBaseHandler {
 public:
  RestIndexHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

  ~RestIndexHandler();

 public:
  char const* name() const override final { return "RestIndexHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;
  RestStatus continueExecute() override;
  void shutdownExecute(bool isFinalized) noexcept override;

 private:
  RestStatus getIndexes();
  [[nodiscard]] futures::Future<futures::Unit> getSelectivityEstimates();
  RestStatus createIndex();
  RestStatus dropIndex();
  RestStatus syncCaches();

  void shutdownBackgroundThread();

  std::shared_ptr<LogicalCollection> collection(std::string const& cName);

  struct CreateInBackgroundData {
    std::unique_ptr<std::thread> thread;
    Result result;
    arangodb::velocypack::Builder response;
  };

  std::mutex _mutex;
  CreateInBackgroundData _createInBackgroundData;
};
}  // namespace arangodb
