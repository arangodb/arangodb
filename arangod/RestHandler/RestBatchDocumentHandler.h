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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////


// current draft at: https://github.com/arangodb/documents/blob/proposal/batch-document-api/FeatureProposals/BatchDocumentApi.md

#ifndef ARANGOD_REST_HANDLER_REST_BATCH_DOCUMENT_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_BATCH_DOCUMENT_HANDLER_H 1

#include "Basics/Common.h"
#include "Cluster/ResultT.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
namespace rest {
namespace batch_document_handler {
class RemoveRequest;
enum class BatchOperation;
}
}

class RestBatchDocumentHandler : public RestVocbaseBaseHandler {
 public:
  RestBatchDocumentHandler(GeneralRequest*, GeneralResponse*);

 public:
  RestStatus execute() override final;
  char const* name() const override final { return "RestBatchDocumentHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }

 protected:
  virtual TRI_col_type_e getCollectionType() const {
    return TRI_COL_TYPE_DOCUMENT;
  }

 private:
  // replaces a document
  void replaceDocumentsAction(std::string const& collection);

  // updates a document
  void updateDocumentsAction(std::string const& collection);

  // deletes a document
  void removeDocumentsAction(std::string const& collection);

  // helper function for replace and update
  bool modifyDocument(bool);

  void doRemoveDocuments(
    std::string const &collection, const rest::batch_document_handler::RemoveRequest &request,
    VPackSlice vpack_request
  );


  void generateBatchResponseSuccess(
      std::vector<OperationResult> const& result,
      std::unique_ptr<VPackBuilder> extra,
      VPackOptions const* options);

  void generateBatchResponseFailed(
      OperationResult const& result,
      std::unique_ptr<VPackBuilder> extra,
      VPackOptions const* options);

  void generateBatchResponse(
      rest::ResponseCode restResponseCode,
      std::unique_ptr<VPackBuilder> result,
      std::unique_ptr<VPackBuilder> extra,
      VPackOptions const* options);
};
}

#endif
