////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB3_RESTREPAIRHANDLER_H
#define ARANGODB3_RESTREPAIRHANDLER_H

#include "GeneralServer/AsyncJobManager.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {
namespace rest {
class AsyncJobManager;
}

class RestRepairHandler : public arangodb::RestBaseHandler {
 public:
  RestRepairHandler(
    GeneralRequest *request,
    GeneralResponse *response
  );

  char const *name() const final {
    return "RestDemoHandler";
  }

  bool isDirect() const override {
    // TODO [tobias] does this do what I think it does?
    return false;
  }

  RestStatus execute() override;

  // TODO [tobias] maybe implement cancel()

 private:
  void handlePost();

  void repairDistributeShardsLike();
};

}


#endif //ARANGODB3_RESTREPAIRHANDLER_H
