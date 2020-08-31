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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_DOCUMENT_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_DOCUMENT_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "Utils/SingleCollectionTransaction.h"

namespace arangodb {
namespace transaction {
class Methods;
}
  
class RestDocumentHandler : public RestVocbaseBaseHandler {
 public:
  RestDocumentHandler(application_features::ApplicationServer&, GeneralRequest*,
                      GeneralResponse*);
  ~RestDocumentHandler();

 public:
  RestStatus execute() override final;
  char const* name() const override final { return "RestDocumentHandler"; }
  RequestLane lane() const override final {
    bool isSyncReplication = false;
    // We do not care for the real value, enough if it is there.
    std::ignore = _request->value(StaticStrings::IsSynchronousReplicationString,
                                  isSyncReplication);
    if (isSyncReplication) {
      return RequestLane::CLIENT_FAST;
    }
    return RequestLane::CLIENT_SLOW;
  }

  void shutdownExecute(bool isFinalized) noexcept override final;

 protected:
  ResultT<std::pair<std::string, bool>> forwardingTarget() override final;

 private:
  // inserts a document
  RestStatus insertDocument();

  // reads a single or all documents
  RestStatus readDocument();

  // reads a single document
  RestStatus readSingleDocument(bool generateBody);

  // reads multiple documents
  RestStatus readManyDocuments();

  // reads a single document head
  RestStatus checkDocument();

  // replaces a document
  RestStatus replaceDocument();

  // updates a document
  RestStatus updateDocument();

  // helper function for replace and update
  RestStatus modifyDocument(bool);

  // removes a document
  RestStatus removeDocument();
  
private:
  std::unique_ptr<transaction::Methods> _activeTrx;
};
}  // namespace arangodb

#endif
