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

#ifndef ARANGOD_REST_HANDLER_REST_BATCH_DOCUMENT_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_BATCH_DOCUMENT_HANDLER_H 1

#include "Basics/Common.h"
#include "Cluster/ResultT.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
class RestBatchDocumentHandler : public RestVocbaseBaseHandler {
 public:
  RestBatchDocumentHandler(GeneralRequest*, GeneralResponse*);

 public:
  RestStatus execute() override final;
  char const* name() const override final {
    return "RestBatchDocumentHandler";
  }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }

 protected:
  virtual TRI_col_type_e getCollectionType() const {
    return TRI_COL_TYPE_DOCUMENT;
  }

 private:
  // replaces a document
  void replaceDocumentsAction();

  // updates a document
  void updateDocumentsAction();

  // deletes a document
  void deleteDocumentsAction();

  // helper function for replace and update
  bool modifyDocument(bool);

  // The tuple contains (documents, patterns, options)
  ResultT<std::tuple<VPackSlice, VPackSlice, OperationOptions>> parseModifyRequest() const;
};
}

#endif
