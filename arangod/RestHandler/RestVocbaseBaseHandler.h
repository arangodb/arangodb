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

#ifndef ARANGOD_REST_HANDLER_REST_VOCBASE_BASE_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_VOCBASE_BASE_HANDLER_H 1

#include "RestHandler/RestBaseHandler.h"

#include "Rest/HttpResponse.h"
#include "RestServer/VocbaseContext.h"
#include "Utils/OperationResult.h"
#include "VocBase/vocbase.h"

struct TRI_vocbase_t;

namespace arangodb {

class VocbaseContext;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
////////////////////////////////////////////////////////////////////////////////

class RestVocbaseBaseHandler : public RestBaseHandler {
  RestVocbaseBaseHandler(RestVocbaseBaseHandler const&) = delete;
  RestVocbaseBaseHandler& operator=(RestVocbaseBaseHandler const&) = delete;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief agency public path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const AGENCY_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief agency private path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const AGENCY_PRIV_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief batch path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const BATCH_PATH;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief collection path
  //////////////////////////////////////////////////////////////////////////////
  
  static std::string const COLLECTION_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cursor path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const CURSOR_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief database path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const DATABASE_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief document path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const DOCUMENT_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edges path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const EDGES_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief endpoint path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const ENDPOINT_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief document import path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const IMPORT_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief index path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const INDEX_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief replication path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const REPLICATION_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief simple query all path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const SIMPLE_QUERY_ALL_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief simple query all keys path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const SIMPLE_QUERY_ALL_KEYS_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief simple batch document lookup path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const SIMPLE_LOOKUP_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief simple batch document removal path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const SIMPLE_REMOVE_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief upload path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const UPLOAD_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief users path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const USERS_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief view path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const VIEW_PATH;

  /// @brief Internal Traverser path

  static std::string const INTERNAL_TRAVERSER_PATH;

 public:
  RestVocbaseBaseHandler(GeneralRequest*, GeneralResponse*);
  ~RestVocbaseBaseHandler();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief assemble a document id from a string and a string
  /// optionally url-encodes
  //////////////////////////////////////////////////////////////////////////////

  std::string assembleDocumentId(std::string const&, std::string const&, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates a HTTP 201 or 202 response
  //////////////////////////////////////////////////////////////////////////////

  void generate20x(arangodb::OperationResult const&, std::string const&,
                   TRI_col_type_e, arangodb::velocypack::Options const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates message for a saved document
  //////////////////////////////////////////////////////////////////////////////

  void generateSaved(arangodb::OperationResult const& result,
                     std::string const& collectionName, TRI_col_type_e type,
                     arangodb::velocypack::Options const*, bool isMultiple);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates deleted message
  //////////////////////////////////////////////////////////////////////////////

  void generateDeleted(arangodb::OperationResult const& result,
                       std::string const& collectionName, TRI_col_type_e type,
                       arangodb::velocypack::Options const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates document not found error message, no transaction info
  //////////////////////////////////////////////////////////////////////////////

  void generateDocumentNotFound(std::string const& /* collection name */,
                                std::string const& /* document key */) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates not implemented
  //////////////////////////////////////////////////////////////////////////////

  void generateNotImplemented(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates forbidden
  //////////////////////////////////////////////////////////////////////////////

  void generateForbidden();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates precondition failed, without transaction info
  ///        DEPRECATED
  //////////////////////////////////////////////////////////////////////////////

  void generatePreconditionFailed(std::string const&, std::string const& key,
                                  TRI_voc_rid_t rev);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates precondition failed, without transaction info
  //////////////////////////////////////////////////////////////////////////////

  void generatePreconditionFailed(arangodb::velocypack::Slice const& slice);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates not modified
  //////////////////////////////////////////////////////////////////////////////

  void generateNotModified(TRI_voc_rid_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates first entry from a result set
  //////////////////////////////////////////////////////////////////////////////

  void generateDocument(arangodb::velocypack::Slice const&, bool,
                        arangodb::velocypack::Options const* options = nullptr);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generate an error message for a transaction error, this method
  /// is used by the others.
  //////////////////////////////////////////////////////////////////////////////

  void generateTransactionError(std::string const&, OperationResult const&,
      std::string const& key, TRI_voc_rid_t = 0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generate an error message for a transaction error
  //////////////////////////////////////////////////////////////////////////////

  void generateTransactionError(std::string const& str, Result const& res,
                                std::string const& key, TRI_voc_rid_t rid = 0){
    generateTransactionError(str,
        OperationResult(res.errorNumber(), res.errorMessage()), key, rid);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generate an error message for a transaction error
  //////////////////////////////////////////////////////////////////////////////

  void generateTransactionError(OperationResult const& result) {
    generateTransactionError("", result, "", 0);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts the revision
  ///
  /// @note @FA{header} must be lowercase.
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_rid_t extractRevision(char const*, bool&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts a boolean parameter value
  //////////////////////////////////////////////////////////////////////////////

  bool extractBooleanParameter(std::string const& name, bool def) const;

  bool extractBooleanParameter(char const* name, bool def) const {
    return extractBooleanParameter(std::string(name), def);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a string parameter value
////////////////////////////////////////////////////////////////////////////////

  void extractStringParameter(std::string const& name, std::string& ret) const;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief request context
  //////////////////////////////////////////////////////////////////////////////

  VocbaseContext* _context;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the vocbase, managed by VocbaseContext do not release
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_t* _vocbase;

 public:
  bool isDirect() const override { return false; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepareExecute, to react to X-Arango-Nolock header
  //////////////////////////////////////////////////////////////////////////////

  virtual void prepareExecute() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finalizeExecute, to react to X-Arango-Nolock header
  //////////////////////////////////////////////////////////////////////////////

  virtual void finalizeExecute() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _nolockHeaderSet
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_set<std::string>* _nolockHeaderSet;
};
}

#endif
