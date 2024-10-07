////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

/// @brief abstract base request handler
class RestVocbaseBaseHandler : public RestBaseHandler {
  RestVocbaseBaseHandler(RestVocbaseBaseHandler const&) = delete;
  RestVocbaseBaseHandler& operator=(RestVocbaseBaseHandler const&) = delete;

 public:
  /// @brief agency public path
  static std::string const AGENCY_PATH;

  /// @brief agency private path
  static std::string const AGENCY_PRIV_PATH;

  /// @brief analyzer path
  static std::string const ANALYZER_PATH;

  /// @brief collection path
  static std::string const COLLECTION_PATH;

  /// @brief cursor path
  static std::string const CURSOR_PATH;

  /// @brief database path
  static std::string const DATABASE_PATH;

  /// @brief document path
  static std::string const DOCUMENT_PATH;

  /// @brief edges path
  static std::string const EDGES_PATH;

  /// @brief gharial graph api path
  static std::string const GHARIAL_PATH;

  /// @brief endpoint path
  static std::string const ENDPOINT_PATH;

  /// @brief document import path
  static std::string const IMPORT_PATH;

  /// @brief index path
  static std::string const INDEX_PATH;

  /// @brief replication path
  static std::string const REPLICATION_PATH;

  /// @brief simple query all path
  static std::string const SIMPLE_QUERY_ALL_PATH;

  /// @brief simple query all keys path
  static std::string const SIMPLE_QUERY_ALL_KEYS_PATH;

  /// @brief simple query by example path
  static std::string const SIMPLE_QUERY_BY_EXAMPLE;

  /// @brief simple batch document lookup path
  static std::string const SIMPLE_LOOKUP_PATH;

  /// @brief simple batch document removal path
  static std::string const SIMPLE_REMOVE_PATH;

  /// @brief tasks path
  static std::string const TASKS_PATH;

  /// @brief upload path
  static std::string const UPLOAD_PATH;

  /// @brief users path
  static std::string const USERS_PATH;

  /// @brief view path
  static std::string const VIEW_PATH;

  /// @brief Internal Traverser path
  static std::string const INTERNAL_TRAVERSER_PATH;

  RestVocbaseBaseHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);
  ~RestVocbaseBaseHandler();

  virtual void cancel() override {
    RestBaseHandler::cancel();
    _context.cancel();
  }

  void prepareExecute(bool isContinue) override;

  void shutdownExecute(bool isFinalized) noexcept override;

 protected:
  /// @brief returns the short id of the server which should handle this request
  ResultT<std::pair<std::string, bool>> forwardingTarget() override;

  /// @brief assemble a document id from a string and a string
  /// optionally url-encodes
  std::string assembleDocumentId(std::string_view collectionName,
                                 std::string_view key, bool urlEncode) const;

  /// @brief generates a HTTP 201 or 202 response
  void generate20x(arangodb::OperationResult const& result,
                   std::string_view collectionName, TRI_col_type_e type,
                   arangodb::velocypack::Options const* options,
                   bool isMultiple, bool silent,
                   rest::ResponseCode waitForSyncResponseCode);

  /// @brief generates conflict error
  void generateConflictError(arangodb::OperationResult const&,
                             bool precFailed = false);

  /// @brief generates not modified
  void generateNotModified(RevisionId);

  /// @brief generates first entry from a result set
  void generateDocument(velocypack::Slice input, bool generateBody,
                        velocypack::Options const* options = nullptr);

  /// @brief generate an error message for a transaction error, this method
  /// is used by the others.
  void generateTransactionError(std::string_view collectionName,
                                OperationResult const& result,
                                std::string_view key = "",
                                RevisionId rid = RevisionId::none());

  /// @brief extracts the revision. "header" must be lowercase.
  RevisionId extractRevision(char const* header, bool& isValid) const;

  /// @brief extracts a string parameter value
  void extractStringParameter(std::string const& name, std::string& ret) const;

  /**
   * @brief Helper to create a new Transaction for a single collection. The
   * helper method will will lock the collection accordingly. It will
   * additionally check if there is a transaction-id header and will make use of
   * an existing transaction if a transaction id is specified. it can also start
   * a new transaction lazily if requested.
   *
   * @param collectionName Name of the collection to be locked
   * @param mode The access mode (READ / WRITE / EXCLUSIVE)
   *
   * @return A freshly created transaction for the given collection with proper
   * locking or a leased transaction.
   */
  futures::Future<std::unique_ptr<transaction::Methods>> createTransaction(
      std::string const& cname, AccessMode::Type mode,
      OperationOptions const& opOptions,
      transaction::OperationOrigin operationOrigin,
      transaction::Options&& trxOpts = transaction::Options()) const;

  /// @brief create proper transaction context, including the proper IDs
  futures::Future<std::shared_ptr<transaction::Context>>
  createTransactionContext(AccessMode::Type mode,
                           transaction::OperationOrigin operationOrigin) const;

 protected:
  /// @brief request context
  VocbaseContext& _context;

  /// @brief the vocbase, managed by VocbaseContext
  TRI_vocbase_t& _vocbase;

 private:
  std::shared_ptr<LogContext::Values> _scopeVocbaseValues;
  LogContext::EntryPtr _logContextVocbaseEntry;
};

}  // namespace arangodb
