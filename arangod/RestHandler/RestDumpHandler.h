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

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <string>
#include <utility>

namespace arangodb {
class ClusterInfo;
class RocksDBDumpManager;

class RestDumpHandler : public RestVocbaseBaseHandler {
 public:
  RestDumpHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

  char const* name() const override final { return "RestDumpHandler"; }

  RequestLane lane() const override final;

  RestStatus execute() override;

 protected:
  ResultT<std::pair<std::string, bool>> forwardingTarget() override final;

 private:
  void handleCommandDumpStart();

  void handleCommandDumpNext();

  void handleCommandDumpFinished();

  std::string getAuthorizedUser() const;

  Result validateRequest();

  RocksDBDumpManager* _dumpManager = nullptr;
  ClusterInfo& _clusterInfo;
};
}  // namespace arangodb
