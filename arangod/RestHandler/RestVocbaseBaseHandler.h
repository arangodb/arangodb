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

#include "Basics/Common.h"
#include "Basics/logging.h"
#include "Rest/GeneralResponse.h"
#include "RestHandler/RestBaseHandler.h"
#include "RestServer/VocbaseContext.h"
#include "Utils/transactions.h"


struct TRI_document_collection_t;
class TRI_vocbase_col_t;
struct TRI_vocbase_t;
class VocShaper;

namespace arangodb {

class VocbaseContext;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
////////////////////////////////////////////////////////////////////////////////

class RestVocbaseBaseHandler : public admin::RestBaseHandler {
 private:
  RestVocbaseBaseHandler(RestVocbaseBaseHandler const&) = delete;
  RestVocbaseBaseHandler& operator=(RestVocbaseBaseHandler const&) = delete;

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief batch path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const BATCH_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cursor path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const CURSOR_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief document path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const DOCUMENT_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edge path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const EDGE_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edges path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const EDGES_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief document export path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const EXPORT_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief document import path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const IMPORT_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief replication path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const REPLICATION_PATH;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief simple query all path
  //////////////////////////////////////////////////////////////////////////////

  static std::string const SIMPLE_QUERY_ALL_PATH;

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

  
 public:

  explicit RestVocbaseBaseHandler(rest::HttpRequest*);


  ~RestVocbaseBaseHandler();

  
 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief check if a collection needs to be created on the fly
  ///
  /// this method will check the "createCollection" attribute of the request. if
  /// it is set to true, it will verify that the named collection actually
  /// exists.
  /// if the collection does not yet exist, it will create it on the fly.
  /// if the "createCollection" attribute is not set or set to false, nothing
  /// will
  /// happen, and the collection name will not be checked
  //////////////////////////////////////////////////////////////////////////////

  bool checkCreateCollection(std::string const&, TRI_col_type_e);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates a HTTP 201 or 202 response
  //////////////////////////////////////////////////////////////////////////////

  void generate20x(rest::HttpResponse::HttpResponseCode, std::string const&,
                   TRI_voc_key_t, TRI_voc_rid_t, TRI_col_type_e);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates ok message without content
  //////////////////////////////////////////////////////////////////////////////

  void generateOk() { createResponse(rest::HttpResponse::NO_CONTENT); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates ok message with no body but with certain status code
  //////////////////////////////////////////////////////////////////////////////

  void generateOk(rest::HttpResponse::HttpResponseCode code) {
    createResponse(code);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates message for a saved document
  //////////////////////////////////////////////////////////////////////////////

  void generateSaved(arangodb::SingleCollectionWriteTransaction<1>& trx,
                     TRI_voc_cid_t cid, TRI_doc_mptr_copy_t const& mptr) {
    TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

    rest::HttpResponse::HttpResponseCode statusCode;
    if (trx.synchronous()) {
      statusCode = rest::HttpResponse::CREATED;
    } else {
      statusCode = rest::HttpResponse::ACCEPTED;
    }

    TRI_col_type_e type = trx.documentCollection()->_info.type();
    generate20x(statusCode, trx.resolver()->getCollectionName(cid),
                (TRI_voc_key_t)TRI_EXTRACT_MARKER_KEY(&mptr), mptr._rid,
                type);  // PROTECTED by trx here
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates deleted message
  //////////////////////////////////////////////////////////////////////////////

  void generateDeleted(
      arangodb::SingleCollectionWriteTransaction<1>& trx,
      TRI_voc_cid_t cid, TRI_voc_key_t key, TRI_voc_rid_t rid) {
    rest::HttpResponse::HttpResponseCode statusCode;
    if (trx.synchronous()) {
      statusCode = rest::HttpResponse::OK;
    } else {
      statusCode = rest::HttpResponse::ACCEPTED;
    }

    TRI_col_type_e type = trx.documentCollection()->_info.type();
    generate20x(statusCode, trx.resolver()->getCollectionName(cid), key, rid,
                type);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates document not found error message, read transaction
  //////////////////////////////////////////////////////////////////////////////

  void generateDocumentNotFound(
      arangodb::SingleCollectionReadOnlyTransaction& trx,
      TRI_voc_cid_t cid, TRI_voc_key_t key) {
    generateDocumentNotFound(trx.resolver()->getCollectionName(cid), key);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates document not found error message, write transaction
  //////////////////////////////////////////////////////////////////////////////

  void generateDocumentNotFound(
      arangodb::SingleCollectionWriteTransaction<1>& trx,
      TRI_voc_cid_t cid, TRI_voc_key_t key) {
    generateDocumentNotFound(trx.resolver()->getCollectionName(cid), key);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates document not found error message, no transaction info
  //////////////////////////////////////////////////////////////////////////////

  void generateDocumentNotFound(std::string const& collectionName,
                                TRI_voc_key_t key) {
    generateError(rest::HttpResponse::NOT_FOUND,
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
  /// @brief generates precondition failed, for a read-transaction
  //////////////////////////////////////////////////////////////////////////////

  void generatePreconditionFailed(SingleCollectionReadOnlyTransaction& trx,
                                  TRI_voc_cid_t cid,
                                  TRI_doc_mptr_copy_t const& mptr,
                                  TRI_voc_rid_t rid) {
    return generatePreconditionFailed(
        trx.resolver()->getCollectionName(cid),
        (TRI_voc_key_t)TRI_EXTRACT_MARKER_KEY(&mptr),
        rid);  // PROTECTED by RUNTIME
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates precondition failed, for a write-transaction
  //////////////////////////////////////////////////////////////////////////////

  void generatePreconditionFailed(SingleCollectionWriteTransaction<1>& trx,
                                  TRI_voc_cid_t cid,
                                  TRI_doc_mptr_copy_t const& mptr,
                                  TRI_voc_rid_t rid) {
    return generatePreconditionFailed(
        trx.resolver()->getCollectionName(cid),
        (TRI_voc_key_t)TRI_EXTRACT_MARKER_KEY(&mptr),
        rid);  // PROTECTED by RUNTIME
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates precondition failed, without transaction info
  //////////////////////////////////////////////////////////////////////////////

  void generatePreconditionFailed(std::string const&, TRI_voc_key_t key,
                                  TRI_voc_rid_t rid);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates not modified
  //////////////////////////////////////////////////////////////////////////////

  void generateNotModified(TRI_voc_rid_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates first entry from a result set
  //////////////////////////////////////////////////////////////////////////////

  void generateDocument(SingleCollectionReadOnlyTransaction& trx, TRI_voc_cid_t,
                        TRI_doc_mptr_copy_t const&, VocShaper*, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generate an error message for a transaction error
  //////////////////////////////////////////////////////////////////////////////

  void generateTransactionError(std::string const&, int, TRI_voc_key_t = 0,
                                TRI_voc_rid_t = 0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts the revision
  ///
  /// @note @FA{header} must be lowercase.
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_rid_t extractRevision(char const*, char const*, bool&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts the update policy
  //////////////////////////////////////////////////////////////////////////////

  TRI_doc_update_policy_e extractUpdatePolicy() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts the waitForSync value
  //////////////////////////////////////////////////////////////////////////////

  bool extractWaitForSync() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses the body as VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<VPackBuilder> parseVelocyPackBody(VPackOptions const*, bool&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses a document handle, on a cluster this will parse the
  /// collection name as a cluster-wide collection name and return a
  /// cluster-wide collection ID in `cid`.
  //////////////////////////////////////////////////////////////////////////////

  int parseDocumentId(arangodb::CollectionNameResolver const*,
                      std::string const&, TRI_voc_cid_t&, TRI_voc_key_t&);

  
 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief request context
  //////////////////////////////////////////////////////////////////////////////

  VocbaseContext* _context;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the vocbase
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
  /// @brief _nolockHeaderFound
  //////////////////////////////////////////////////////////////////////////////

  bool _nolockHeaderFound;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _nolockHeaderFound
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_set<std::string>* _nolockHeaderSet;
};
}

#endif


