////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <string>
#include <utility>

namespace arangodb {
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
  RocksDBDumpManager* _manager;

  void handleCommandDumpStart();

  void handleCommandDumpNext();

  void handleCommandDumpFinished();

  std::string getAuthorizedUser() const;
};
}  // namespace arangodb
