////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_VIEW_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_VIEW_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief view request handler
////////////////////////////////////////////////////////////////////////////////

class RestViewHandler : public RestVocbaseBaseHandler {
 public:
  RestViewHandler(application_features::ApplicationServer&, GeneralRequest*, GeneralResponse*);

 public:
  char const* name() const override final { return "RestViewHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  virtual RestStatus execute() override;

 protected:
  void createView();
  void modifyView(bool partialUpdate);
  void deleteView();
  void getView(std::string const& nameOrId, bool detailed);
  void getViews();
};

}  // namespace arangodb

#endif