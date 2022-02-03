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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "Pregel3/Methods.h"

namespace arangodb {
using namespace pregel3;

class RestPregel3Handler : public arangodb::RestVocbaseBaseHandler {
 public:
  explicit RestPregel3Handler(ArangodServer&, GeneralRequest*,
                              GeneralResponse*);

  ~RestPregel3Handler() override;

 private:
  auto executeByMethod(pregel3::Pregel3Methods const& methods) -> RestStatus;
  /**
   * Possible inputs are
   * (1) .../_api/pregel3/queries/<queryId> : get the status
   * (2) .../_api/pregel3/queries/<queryId>/loadGraph : load the graph
   * (3) .../_api/pregel3/queries/<queryId>/start : start the computation
   * (4) .../_api/pregel3/queries/<queryId>/store : store the results
   * For all other inputs returns 404.
   * @param methods
   * @return
   */
  auto handleGetRequest(pregel3::Pregel3Methods const& methods) -> RestStatus;
  auto handlePostRequest(pregel3::Pregel3Methods const& methods) -> RestStatus;
  auto _generateErrorWrongInput(std::string const& info = "") -> void;

 public:
  RestStatus execute() override;
  char const* name() const override { return "Pregel3 Rest Handler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }

 private:
  Pregel3Feature& _pregel3Feature;
  bool _ensureQueryId(const VPackSlice& body,
                      pregel3::Pregel3Methods const& methods,
                      std::string& queryId);
  bool _getPostBody(VPackSlice& body);
};
}  // namespace arangodb
