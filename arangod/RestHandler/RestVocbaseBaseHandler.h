////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "RestHandler/RestBaseHandler.h"

#include "Rest/GeneralResponse.h"
#include "RestServer/VocbaseContext.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/Options.h"
#include "Utils/OperationResult.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/vocbase.h"

#include <memory>
#include <string_view>

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
class Methods;
}

class VocbaseContext;

// abstract base request handler
class RestVocbaseBaseHandler : public RestBaseHandler {
  RestVocbaseBaseHandler(RestVocbaseBaseHandler const&) = delete;
  RestVocbaseBaseHandler& operator=(RestVocbaseBaseHandler const&) = delete;

 public:
  // access token path
  static std::string const ACCESS_TOKEN_PATH;

  // agency public path
  static std::string const AGENCY_PATH;

  // agency private path
  static std::string const AGENCY_PRIV_PATH;

  // analyzer path
  static std::string const ANALYZER_PATH;

  // collection path
  static std::string const COLLECTION_PATH;

  // cursor path
  static std::string const CURSOR_PATH;

  // database path
  static std::string const DATABASE_PATH;

  // document path
  static std::string const DOCUMENT_PATH;

  // edges path
  static std::string const EDGES_PATH;

  // gharial graph api path
  static std::string const GHARIAL_PATH;

  // endpoint path
  static std::string const ENDPOINT_PATH;

  // document import path
  static std::string const IMPORT_PATH;

  // index path
  static std::string const INDEX_PATH;

  // replication path
  static std::string const REPLICATION_PATH;

  // schema path
  static std::string const SCHEMA_PATH;

  // simple query all path
  static std::string const SIMPLE_QUERY_ALL_PATH;

  // simple query all keys path
  static std::string const SIMPLE_QUERY_ALL_KEYS_PATH;

  // simple query by example path
  static std::string const SIMPLE_QUERY_BY_EXAMPLE;

  // simple batch document lookup path
  static std::string const SIMPLE_LOOKUP_PATH;

  // simple batch document removal path
  static std::string const SIMPLE_REMOVE_PATH;

  // tasks path
  static std::string const TASKS_PATH;

  // upload path
  static std::string const UPLOAD_PATH;

  // users path
  static std::string const USERS_PATH;

  // view path
  static std::string const VIEW_PATH;

  // Internal Traverser path
  static std::string const INTERNAL_TRAVERSER_PATH;

  static std::string const TRAVERSER_PATH_EDGE;

  static std::string const TRAVERSER_PATH_VERTEX;

  RestVocbaseBaseHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);
  ~RestVocbaseBaseHandler();

  virtual void cancel() override {
    RestBaseHandler::cancel();
    _context.cancel();
  }

 protected:
  // returns the short id of the server which should handle this request
  ResultT<std::pair<std::string, bool>> forwardingTarget() override;

  // assemble a document id from a string and a string
  /// optionally url-encodes
  std::string assembleDocumentId(std::string_view collectionName,
                                 std::string_view key, bool urlEncode) const;

  // generates a HTTP 201 or 202 response
  void generate20x(arangodb::OperationResult const& result,
                   std::string_view collectionName, TRI_col_type_e type,
                   arangodb::velocypack::Options const* options,
                   bool isMultiple, bool silent,
                   rest::ResponseCode waitForSyncResponseCode);

  // generates conflict error
  void generateConflictError(arangodb::OperationResult const&,
                             bool precFailed = false);

  // generates not modified
  void generateNotModified(RevisionId);

  // generates first entry from a result set
  void generateDocument(velocypack::Slice input, bool generateBody,
                        velocypack::Options const* options = nullptr);

  // generate an error message for a transaction error, this method
  /// is used by the others.
  void generateTransactionError(std::string_view collectionName,
                                OperationResult const& result,
                                std::string_view key = "",
                                RevisionId rid = RevisionId::none());

  // extracts the revision. "header" must be lowercase.
  RevisionId extractRevision(char const* header, bool& isValid) const;

  // extracts a string parameter value
  void extractStringParameter(std::string const& name, std::string& ret) const;

  // Helper to create a new Transaction for a single collection. The
  // helper method will will lock the collection accordingly. It will
  // additionally check if there is a transaction-id header and will make use of
  // an existing transaction if a transaction id is specified. it can also start
  // a new transaction lazily if requested.
  //
  // collectionName: Name of the collection to be locked
  // mode: The access mode (READ / WRITE / EXCLUSIVE)
  //
  // Returns a freshly created transaction for the given collection with proper
  // locking or a leased transaction.

  futures::Future<std::unique_ptr<transaction::Methods>> createTransaction(
      std::string const& cname, AccessMode::Type mode,
      OperationOptions const& opOptions,
      transaction::OperationOrigin operationOrigin,
      transaction::Options&& trxOpts = transaction::Options()) const;

  // create proper transaction context, including the proper IDs
  futures::Future<std::shared_ptr<transaction::Context>>
  createTransactionContext(AccessMode::Type mode,
                           transaction::OperationOrigin operationOrigin) const;

 protected:
  // Please see the comment in RestHandler::makeSharedLogContextValue() for
  // some comments.
  [[nodiscard]] auto makeSharedLogContextValue() const
      -> std::shared_ptr<LogContext::Values> override {
    return LogContext::makeValue()
        .with<structuredParams::UrlName>(_request->fullUrl())
        .with<structuredParams::UserName>(_request->user())
        .with<structuredParams::DatabaseName>(_vocbase.name())
        .share();
  }

 protected:
  // request context
  VocbaseContext& _context;

  // the vocbase, managed by VocbaseContext
  TRI_vocbase_t& _vocbase;
};

}  // namespace arangodb
