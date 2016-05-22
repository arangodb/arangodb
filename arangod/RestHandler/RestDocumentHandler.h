////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

namespace arangodb {
class RestDocumentHandler : public RestVocbaseBaseHandler {
 public:
  RestDocumentHandler(GeneralRequest*, GeneralResponse*);

 public:
  status execute() override final;

 protected:
  virtual TRI_col_type_e getCollectionType() const {
    return TRI_COL_TYPE_DOCUMENT;
  }

  // creates a document
  virtual bool createDocument();

  // reads a single or all documents
  bool readDocument();

  // reads a single document
  bool readSingleDocument(bool generateBody);

  // reads multiple documents
  bool readManyDocuments();

  // reads a single document head
  bool checkDocument();

  // replaces a document
  bool replaceDocument();

  // updates a document
  bool updateDocument();

  // helper function for replace and update
  bool modifyDocument(bool);

  // deletes a document
  bool deleteDocument();

};
}

#endif
