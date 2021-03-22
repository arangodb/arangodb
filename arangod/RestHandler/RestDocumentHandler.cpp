////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "RestDocumentHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDocumentHandler::RestDocumentHandler(application_features::ApplicationServer& server,
                                         GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestDocumentHandler::~RestDocumentHandler() = default;

RestStatus RestDocumentHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::DELETE_REQ:
      return removeDocument();
    case rest::RequestType::GET:
      return readDocument();
    case rest::RequestType::HEAD:
      return checkDocument();
    case rest::RequestType::POST:
      return insertDocument();
    case rest::RequestType::PUT:
      return replaceDocument();
    case rest::RequestType::PATCH:
      return updateDocument();
    default: { generateNotImplemented("ILLEGAL " + DOCUMENT_PATH); }
  }

  // this handler is done
  return RestStatus::DONE;
}

void RestDocumentHandler::shutdownExecute(bool isFinalized) noexcept {
  if (isFinalized) {
    // reset the transaction so it releases all locks as early as possible
    _activeTrx.reset();

    try {
      GeneralRequest const* request = _request.get();
      auto const type = request->requestType();
      auto const result = _response->responseCode();

      switch (type) {
        case rest::RequestType::DELETE_REQ:
        case rest::RequestType::GET:
        case rest::RequestType::HEAD:
        case rest::RequestType::POST:
        case rest::RequestType::PUT:
        case rest::RequestType::PATCH:
          break;
        default:
          events::IllegalDocumentOperation(*request, result);
          break;
      }
    } catch (...) {
    }
  }

  RestVocbaseBaseHandler::shutdownExecute(isFinalized);
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestDocumentHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  bool found = false;
  std::string const& value = _request->header(StaticStrings::TransactionId, found);
  if (found) {
    TransactionId tid{basics::StringUtils::uint64(value)};
    if (!tid.isCoordinatorTransactionId()) {
      TRI_ASSERT(tid.isLegacyTransactionId());
      return {std::make_pair(StaticStrings::Empty, false)};
    }
    uint32_t sourceServer = tid.serverId();
    if (sourceServer == ServerState::instance()->getShortId()) {
      return {std::make_pair(StaticStrings::Empty, false)};
    }
    auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
    return {std::make_pair(ci.getCoordinatorByShortID(sourceServer), false)};
  }

  return {std::make_pair(StaticStrings::Empty, false)};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_CREATE
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::insertDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() > 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    return RestStatus::DONE;
  }

  bool found;
  std::string cname;
  if (suffixes.size() == 1) {
    cname = suffixes[0];
    found = true;
  } else {
    cname = _request->value("collection", found);
  }

  if (!found || cname.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH +
                  " POST /_api/document/<collection> or query parameter 'collection'");
    return RestStatus::DONE;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }


  arangodb::OperationOptions opOptions(_context);
  opOptions.isRestore = _request->parsedValue(StaticStrings::IsRestoreString, false);
  opOptions.waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  opOptions.validate = !_request->parsedValue(StaticStrings::SkipDocumentValidation, false);
  opOptions.returnNew = _request->parsedValue(StaticStrings::ReturnNewString, false);
  opOptions.silent = _request->parsedValue(StaticStrings::SilentString, false);
  
  if (_request->parsedValue(StaticStrings::Overwrite, false)) {
    // the default behavior if just "overwrite" is set
    opOptions.overwriteMode = OperationOptions::OverwriteMode::Replace;
  }


  std::string const& mode = _request->value(StaticStrings::OverwriteMode);
  if (!mode.empty()) {
    auto overwriteMode = OperationOptions::determineOverwriteMode(velocypack::StringRef(mode));

    if (overwriteMode != OperationOptions::OverwriteMode::Unknown) {
      opOptions.overwriteMode = overwriteMode;

      if (opOptions.overwriteMode == OperationOptions::OverwriteMode::Update) {
        opOptions.mergeObjects = _request->parsedValue(StaticStrings::MergeObjectsString, true);
        opOptions.keepNull = _request->parsedValue(StaticStrings::KeepNullString, false);
      }
    }
  }
  opOptions.returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false) &&
                        opOptions.isOverwriteModeUpdateReplace();
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);

  // find and load collection given by name or identifier
  _activeTrx = createTransaction(cname, AccessMode::Type::WRITE, opOptions);
  bool const isMultiple = body.isArray();

  if (!isMultiple && !opOptions.isOverwriteModeUpdateReplace()) {
    _activeTrx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = _activeTrx->begin();
  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    return RestStatus::DONE;
  }

  return waitForFuture(
      _activeTrx->insertAsync(cname, body, opOptions)
          .thenValue([=](OperationResult&& opres) {
            // Will commit if no error occured.
            // or abort if an error occured.
            // result stays valid!
            return _activeTrx->finishAsync(opres.result).thenValue([=, opres(std::move(opres))](Result&& res) {
              if (opres.fail()) {
                generateTransactionError(cname, opres);
                return;
              }

              if (res.fail()) {
                generateTransactionError(cname, OperationResult(res, opOptions),
                                         "");
                return;
              }

              generateSaved(opres, cname,
                            TRI_col_type_e(_activeTrx->getCollectionType(cname)),
                            _activeTrx->transactionContextPtr()->getVPackOptions(),
                            isMultiple);
            });
          }));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::readDocument() {
  size_t const len = _request->suffixes().size();

  switch (len) {
    case 0:
    case 1:
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    "expecting GET /_api/document/<collection>/<key>");
      return RestStatus::DONE;
    case 2:
      return readSingleDocument(true);

    default:
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "expecting GET /_api/document/<collection>/<key>");
      return RestStatus::DONE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::readSingleDocument(bool generateBody) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  // split the document reference
  std::string const& collection = suffixes[0];
  std::string const& key = suffixes[1];

  // check for an etag
  bool isValidRevision;
  RevisionId ifNoneRid = extractRevision("if-none-match", isValidRevision);
  if (!isValidRevision) {
    ifNoneRid = RevisionId::max();  // an impossible rev, so precondition failed will happen
  }

  OperationOptions options(_context);
  options.ignoreRevs = true;

  RevisionId ifRid = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    ifRid = RevisionId::max();  // an impossible rev, so precondition failed will happen
  }

  auto buffer = std::make_shared<VPackBuffer<uint8_t>>();
  VPackBuilder builder(buffer);
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(key));
    if (ifRid.isSet()) {
      options.ignoreRevs = false;
      builder.add(StaticStrings::RevString, VPackValue(ifRid.toString()));
    }
  }

  VPackSlice search = builder.slice();

  // find and load collection given by name or identifier
  _activeTrx = createTransaction(collection, AccessMode::Type::READ, options);

  _activeTrx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  // ...........................................................................
  // inside read transaction
  // ...........................................................................

  Result res = _activeTrx->begin();

  if (!res.ok()) {
    generateTransactionError(collection, OperationResult(res, options), "");
    return RestStatus::DONE;
  }

  return waitForFuture(
      _activeTrx->documentAsync(collection, search, options)
          .thenValue([=, buffer(std::move(buffer))](OperationResult opRes) {
            return _activeTrx->finishAsync(opRes.result)
                .thenValue([=, opRes(std::move(opRes))](Result&& res) {
                  if (!opRes.ok()) {
                    generateTransactionError(collection, opRes, key, ifRid);
                    return;
                  }

                  if (!res.ok()) {
                    generateTransactionError(collection, OperationResult(res, options),
                                             key, ifRid);
                    return;
                  }

                  if (ifNoneRid.isSet()) {
                    RevisionId const rid = RevisionId::fromSlice(opRes.slice());
                    if (ifNoneRid == rid) {
                      generateNotModified(rid);
                      return;
                    }
                  }

                  // use default options
                  generateDocument(opRes.slice(), generateBody,
                                   _activeTrx->transactionContextPtr()->getVPackOptions());
                });
          }));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ_HEAD
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::checkDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting HEAD /_api/document/<collection>/<key>");
    return RestStatus::DONE;
  }

  return readSingleDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_REPLACE
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::replaceDocument() {
  bool found;
  _request->value("onlyget", found);
  if (found) {
    return readManyDocuments();
  }
  return modifyDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_UPDATE
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::updateDocument() { return modifyDocument(true); }

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replaceDocument and updateDocument
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::modifyDocument(bool isPatch) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() > 2) {
    std::string msg("expecting ");
    msg.append(isPatch ? "PATCH" : "PUT");
    msg.append(
        " /_api/document/<collection> or"
        " /_api/document/<collection>/<key> or"
        " /_api/document and query parameter 'collection'");

    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
    return RestStatus::DONE;
  }

  bool isArrayCase = suffixes.size() <= 1;

  std::string cname;
  std::string key;

  if (isArrayCase) {
    bool found;
    if (suffixes.size() == 1) {
      cname = suffixes[0];
      found = true;
    } else {
      cname = _request->value("collection", found);
    }
    if (!found) {
      std::string msg(
          "collection must be given in URL path or query parameter "
          "'collection' must be specified");
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
      return RestStatus::DONE;
    }
  } else {
    cname = suffixes[0];
    key = suffixes[1];
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  OperationOptions opOptions(_context);
  if ((!isArrayCase && !body.isObject()) || (isArrayCase && !body.isArray())) {
    generateTransactionError(cname, OperationResult(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID, opOptions),
                             "");
    return RestStatus::DONE;
  }

  opOptions.isRestore = _request->parsedValue(StaticStrings::IsRestoreString, false);
  opOptions.ignoreRevs = _request->parsedValue(StaticStrings::IgnoreRevsString, true);
  opOptions.waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  opOptions.validate = !_request->parsedValue(StaticStrings::SkipDocumentValidation, false);
  opOptions.returnNew = _request->parsedValue(StaticStrings::ReturnNewString, false);
  opOptions.returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);
  opOptions.silent = _request->parsedValue(StaticStrings::SilentString, false);
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);

  // extract the revision, if single document variant and header given:
  std::shared_ptr<VPackBuffer<uint8_t>> buffer;
  RevisionId headerRev = RevisionId::none();
  if (!isArrayCase) {
    bool isValidRevision;
    headerRev = extractRevision("if-match", isValidRevision);
    if (!isValidRevision) {
      headerRev = RevisionId::max();  // an impossible revision, so precondition failed
    }
    if (headerRev.isSet()) {
      opOptions.ignoreRevs = false;
    }

    VPackSlice keyInBody = body.get(StaticStrings::KeyString);
    RevisionId revInBody = RevisionId::fromSlice(body);
    if ((headerRev.isSet() && revInBody != headerRev) || keyInBody.isNone() ||
        keyInBody.isNull() || (keyInBody.isString() && keyInBody.copyString() != key)) {
      // We need to rewrite the document with the given revision and key:
      buffer = std::make_shared<VPackBuffer<uint8_t>>();
      VPackBuilder builder(buffer);
      {
        VPackObjectBuilder guard(&builder);
        TRI_SanitizeObject(body, builder);
        builder.add(StaticStrings::KeyString, VPackValue(key));
        if (headerRev.isSet()) {
          builder.add(StaticStrings::RevString, VPackValue(headerRev.toString()));
        } else if (!opOptions.ignoreRevs && revInBody.isSet()) {
          builder.add(StaticStrings::RevString, VPackValue(revInBody.toString()));
          headerRev = revInBody;   // make sure that we report 412 and not 409
        }
      }

      body = builder.slice();
    } else if (!headerRev.isSet() && revInBody.isSet() &&
               opOptions.ignoreRevs == false) {
      headerRev = revInBody;   // make sure that we report 412 and not 409
    }
  }

  // find and load collection given by name or identifier
  _activeTrx = createTransaction(cname, AccessMode::Type::WRITE, opOptions);

  if (!isArrayCase) {
    _activeTrx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  // ...........................................................................
  // inside write transaction
  // ...........................................................................

  Result res = _activeTrx->begin();
  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    return RestStatus::DONE;
  }

  auto f = futures::Future<OperationResult>::makeEmpty();
  if (isPatch) {
    // patching an existing document
    opOptions.keepNull = _request->parsedValue(StaticStrings::KeepNullString, true);
    opOptions.mergeObjects =
        _request->parsedValue(StaticStrings::MergeObjectsString, true);
    f = _activeTrx->updateAsync(cname, body, opOptions);
  } else {
    f = _activeTrx->replaceAsync(cname, body, opOptions);
  }

  return waitForFuture(std::move(f).thenValue([=, buffer(std::move(buffer))](OperationResult opRes) {
    return _activeTrx->finishAsync(opRes.result).thenValue([=, opRes(std::move(opRes))](Result&& res) {
      // ...........................................................................
      // outside write transaction
      // ...........................................................................

      if (opRes.fail()) {
        generateTransactionError(cname, opRes, key, headerRev);
        return;
      }

      if (!res.ok()) {
        generateTransactionError(cname, OperationResult(res, opOptions), key, headerRev);
        return;
      }

      generateSaved(opRes, cname, TRI_col_type_e(_activeTrx->getCollectionType(cname)),
                    _activeTrx->transactionContextPtr()->getVPackOptions(), isArrayCase);
    });
  }));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_DELETE
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::removeDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() < 1 || suffixes.size() > 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/document/<collection>/<key> or "
                  "/_api/document/<collection> with a body");
    return RestStatus::DONE;
  }

  // split the document reference
  std::string const& cname = suffixes[0];
  std::string key;
  if (suffixes.size() == 2) {
    key = suffixes[1];
  }

  // extract the revision if single document case
  RevisionId revision = RevisionId::none();
  if (suffixes.size() == 2) {
    bool isValidRevision = false;
    revision = extractRevision("if-match", isValidRevision);
    if (!isValidRevision) {
      revision = RevisionId::max();  // an impossible revision, so precondition failed
    }
  }

  OperationOptions opOptions(_context);
  opOptions.returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);
  opOptions.ignoreRevs = _request->parsedValue(StaticStrings::IgnoreRevsString, true);
  opOptions.waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  opOptions.silent = _request->parsedValue(StaticStrings::SilentString, false);
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);

  VPackSlice search;
  std::shared_ptr<VPackBuffer<uint8_t>> buffer;

  if (suffixes.size() == 2) {
    buffer = std::make_shared<VPackBuffer<uint8_t>>();
    VPackBuilder builder(buffer);
    {
      VPackObjectBuilder guard(&builder);

      builder.add(StaticStrings::KeyString, VPackValue(key));

      if (revision.isSet()) {
        opOptions.ignoreRevs = false;
        builder.add(StaticStrings::RevString, VPackValue(revision.toString()));
      }
    }

    search = builder.slice();
  } else {
    bool parseSuccess = false;
    search = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {  // error message generated in parseVPackBody
      return RestStatus::DONE;
    }
  }

  if (!search.isArray() && !search.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Request body not parseable");
    return RestStatus::DONE;
  }

  _activeTrx = createTransaction(cname, AccessMode::Type::WRITE, opOptions);
  if (suffixes.size() == 2 || !search.isArray()) {
    _activeTrx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = _activeTrx->begin();

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    return RestStatus::DONE;
  }

  bool const isMultiple = search.isArray();

  return waitForFuture(
      _activeTrx->removeAsync(cname, search, opOptions).thenValue([=, buffer(std::move(buffer))](OperationResult opRes) {
        return _activeTrx->finishAsync(opRes.result).thenValue([=, opRes(std::move(opRes))](Result&& res) {
          // ...........................................................................
          // outside write transaction
          // ...........................................................................

          if (opRes.fail()) {
            generateTransactionError(cname, opRes, key, revision);
            return;
          }

          if (!res.ok()) {
            generateTransactionError(cname, OperationResult(res, opOptions), key);
            return;
          }

          generateDeleted(opRes, cname,
                          TRI_col_type_e(_activeTrx->getCollectionType(cname)),
                          _activeTrx->transactionContextPtr()->getVPackOptions(), isMultiple);
        });
      }));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ_MANY
////////////////////////////////////////////////////////////////////////////////

RestStatus RestDocumentHandler::readManyDocuments() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/document/<collection> with a body");
    return RestStatus::DONE;
  }

  // split the document reference
  std::string const& cname = suffixes[0];

  OperationOptions opOptions(_context);
  opOptions.ignoreRevs = _request->parsedValue(StaticStrings::IgnoreRevsString, true);

  _activeTrx = createTransaction(cname, AccessMode::Type::READ, opOptions);

  // ...........................................................................
  // inside read transaction
  // ...........................................................................

  Result res = _activeTrx->begin();

  if (!res.ok()) {
    generateTransactionError(cname, OperationResult(res, opOptions), "");
    return RestStatus::DONE;
  }

  bool success;
  VPackSlice const search = this->parseVPackBody(success);
  if (!success) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  return waitForFuture(_activeTrx->documentAsync(cname, search, opOptions).thenValue([=](OperationResult opRes) {
    return _activeTrx->finishAsync(opRes.result).thenValue([=, opRes(std::move(opRes))](Result&& res) {
      if (opRes.fail()) {
        generateTransactionError(cname, opRes);
        return;
      }

      if (!res.ok()) {
        generateTransactionError(cname, OperationResult(res, opOptions), "");
        return;
      }

      generateDocument(opRes.slice(), true,
                       _activeTrx->transactionContextPtr()->getVPackOptions());
    });
  }));
}
