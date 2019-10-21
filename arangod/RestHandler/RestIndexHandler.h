////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_INDEX_HANDLER_H
#define ARANGOD_REST_HANDLER_INDEX_HANDLER_H 1

#include <memory>
#include <string>
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
class LogicalCollection;

class RestIndexHandler : public arangodb::RestVocbaseBaseHandler {
 public:
  RestIndexHandler(application_features::ApplicationServer&, GeneralRequest*,
                   GeneralResponse*);

 public:
  char const* name() const override final { return "RestIndexHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;

 private:
  RestStatus getIndexes();
  RestStatus getSelectivityEstimates();
  RestStatus createIndex();
  RestStatus dropIndex();

  std::shared_ptr<LogicalCollection> collection(std::string const& cName);
};
}  // namespace arangodb

#endif
