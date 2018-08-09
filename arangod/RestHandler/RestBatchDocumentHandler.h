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
#include "Transaction/BatchRequests.h"

#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

namespace arangodb {
namespace rest {
namespace batch_document_handler {
struct BatchRequest;
}
}

struct ExtraInformation {
  std::string errorMessage = "";
  int errorNumber = TRI_ERROR_NO_ERROR;
  rest::ResponseCode code = rest::ResponseCode::ACCEPTED ;
  std::size_t errorDataIndex;
  bool isATransactionError = false;

  void addToOpenOject(VPackBuilder& builder){
    builder.add(StaticStrings::Error, VPackValue(errorOccured()));
    if(errorOccured()) {
      builder.add(StaticStrings::ErrorMessage, VPackValue(errorMessage));
      if(!isATransactionError) {
        builder.add("errorDataIndex", VPackValue(errorDataIndex));
      }
    }
  }
  bool errorOccured(){ return TRI_ERROR_NO_ERROR != errorNumber; }
};

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
  // helper function for replace and update
  bool modifyDocument(bool);

  template <typename T>
  void executeBatchRequest(
      std::string const& collection, batch::Request<T>&& request) {

    std::vector<OperationResult> opResults; // will hold the result

    velocypack::Options vOptions;
    std::shared_ptr<velocypack::CustomTypeHandler> vCustomHandler; // we need to store the shared pointer to the CustomTypeHandler
                                                                   // otherwise the options copy will become invalid
    bool vOptionsSet = false;
    VPackBuilder builder;



    auto trx = createTransaction(collection, AccessMode::Type::WRITE);
    if(!vOptionsSet){
      vOptions = *(trx->transactionContextPtr()->getVPackOptionsForDump());
      vCustomHandler= trx->transactionContextPtr()->orderCustomTypeHandler();
    }

    Result transactionResult = trx->begin();

    if (transactionResult.fail()) {
      opResults.push_back(OperationResult(transactionResult));
    }


    OperationResult operationResult = request.execute(trx.get(), collection);
    opResults.push_back(std::move(operationResult));


    generateBatchResponse(
        opResults,
        &vOptions
    );
  }

  void generateBatchResponse(
      std::vector<OperationResult> const& result,
      VPackOptions const* options);

  void generateBatchResponse(
      std::unique_ptr<VPackBuilder> result,
      ExtraInformation&&,
      VPackOptions const* options);

  void generateBatchResponse(
      rest::ResponseCode restResponseCode,
      VPackSlice result,
      VPackOptions const* options);
};
}

#endif
