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
template<typename>
struct async;

class RestIndexHandler : public arangodb::RestVocbaseBaseHandler {
 public:
  RestIndexHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

  char const* name() const final { return "RestIndexHandler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }
  futures::Future<futures::Unit> executeAsync() final;

 private:
  async<void> getIndexes();
  [[nodiscard]] futures::Future<futures::Unit> getSelectivityEstimates();
  async<void> createIndex();
  async<void> dropIndex();
  void syncCaches();

  std::shared_ptr<LogicalCollection> collection(std::string const& cName);
};
}  // namespace arangodb
