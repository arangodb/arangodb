////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterRestExportHandler.h"

using namespace arangodb;
using namespace arangodb::rest;

ClusterRestExportHandler::ClusterRestExportHandler(application_features::ApplicationServer& server,
                                                   GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus ClusterRestExportHandler::execute() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
                "'/_api/export' is not supported in a cluster");
  return RestStatus::DONE;
}
