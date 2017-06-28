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

#include "RestDocumentHandler.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/HttpRequest.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDocumentHandler::RestDocumentHandler(GeneralRequest* request,
                                         GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus RestDocumentHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::DELETE_REQ:
      deleteDocument();
      break;
    case rest::RequestType::GET:
      readDocument();
      break;
    case rest::RequestType::HEAD:
      checkDocument();
      break;
    case rest::RequestType::POST:
      createDocument();
      break;
    case rest::RequestType::PUT:
      replaceDocument();
      break;
    case rest::RequestType::PATCH:
      updateDocument();
      break;
    default: { generateNotImplemented("ILLEGAL " + DOCUMENT_PATH); }
  }

  // this handler is done
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_CREATE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() > 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool found;
  std::string collectionName;
  if (suffixes.size() == 1) {
    collectionName = suffixes[0];
    found = true;
  } else {
    collectionName = _request->value("collection", found);
  }

  if (!found || collectionName.empty()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH +
                      "/<collectionname> or query parameter 'collection'");
    return false;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    return false;
  }

  VPackSlice body = parsedBody->slice();

  arangodb::OperationOptions opOptions;
  opOptions.isRestore =
      extractBooleanParameter(StaticStrings::IsRestoreString, false);
  opOptions.waitForSync =
      extractBooleanParameter(StaticStrings::WaitForSyncString, false);
  opOptions.returnNew =
      extractBooleanParameter(StaticStrings::ReturnNewString, false);
  opOptions.silent =
      extractBooleanParameter(StaticStrings::SilentString, false);
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);

  // find and load collection given by name or identifier
  auto transactionContext(transaction::StandaloneContext::Create(_vocbase));
  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::WRITE);
  bool const isMultiple = body.isArray();
  if (!isMultiple) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();

  if (!res.ok()) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  arangodb::OperationResult result =
      trx.insert(collectionName, body, opOptions);

  // Will commit if no error occured.
  // or abort if an error occured.
  // result stays valid!
  res = trx.finish(result.code);

  if (result.failed()) {
    generateTransactionError(result);
    return false;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  generateSaved(result, collectionName,
                TRI_col_type_e(trx.getCollectionType(collectionName)),
                transactionContext->getVPackOptionsForDump(), isMultiple);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readDocument() {
  size_t const len = _request->suffixes().size();

  switch (len) {
    case 0:
    case 1:
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                    "expecting GET /_api/document/<document-handle>");
      return false;
    case 2:
      return readSingleDocument(true);

    default:
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "expecting GET /_api/document/<document-handle>");
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readSingleDocument(bool generateBody) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  // split the document reference
  std::string const& collection = suffixes[0];
  std::string const& key = suffixes[1];

  // check for an etag
  bool isValidRevision;
  TRI_voc_rid_t ifNoneRid = extractRevision("if-none-match", isValidRevision);
  if (!isValidRevision) {
    ifNoneRid =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }

  OperationOptions options;
  options.ignoreRevs = true;

  TRI_voc_rid_t ifRid = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    ifRid =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(key));
    if (ifRid != 0) {
      options.ignoreRevs = false;
      builder.add(StaticStrings::RevString, VPackValue(TRI_RidToString(ifRid)));
    }
  }
  VPackSlice search = builder.slice();

  // find and load collection given by name or identifier
  auto transactionContext(transaction::StandaloneContext::Create(_vocbase));
  SingleCollectionTransaction trx(transactionContext, collection,
                                  AccessMode::Type::READ);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  // ...........................................................................
  // inside read transaction
  // ...........................................................................

  Result res = trx.begin();

  if (!res.ok()) {
    generateTransactionError(collection, res, "");
    return false;
  }

  OperationResult result = trx.document(collection, search, options);

  res = trx.finish(result.code);

  if (!result.successful()) {
    if (result.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      generateDocumentNotFound(collection, key);
      return false;
    } else if (ifRid != 0 && result.code == TRI_ERROR_ARANGO_CONFLICT) {
      generatePreconditionFailed(result.slice());
    } else {
      generateTransactionError(collection, res, key);
    }
    return false;
  }

  if (!res.ok()) {
    generateTransactionError(collection, res, key);
    return false;
  }

  if (ifNoneRid != 0) {
    TRI_voc_rid_t const rid = TRI_ExtractRevisionId(result.slice());
    if (ifNoneRid == rid) {
      generateNotModified(rid);
      return true;
    }
  }

  // use default options
  generateDocument(result.slice(), generateBody,
                   transactionContext->getVPackOptionsForDump());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ_HEAD
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::checkDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting URI /_api/document/<document-handle>");
    return false;
  }

  return readSingleDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_REPLACE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::replaceDocument() {
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

bool RestDocumentHandler::updateDocument() { return modifyDocument(true); }

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replaceDocument and updateDocument
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::modifyDocument(bool isPatch) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() > 2) {
    std::string msg("expecting ");
    msg.append(isPatch ? "PATCH" : "PUT");
    msg.append(
        " /_api/document/<collectionname> or "
        "/_api/document/<document-handle> "
        "or /_api/document and query parameter 'collection'");

    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
    return false;
  }

  bool isArrayCase = suffixes.size() <= 1;

  std::string collectionName;
  std::string key;

  if (isArrayCase) {
    bool found;
    if (suffixes.size() == 1) {
      collectionName = suffixes[0];
      found = true;
    } else {
      collectionName = _request->value("collection", found);
    }
    if (!found) {
      std::string msg(
          "collection must be given in URL path or query parameter "
          "'collection' must be specified");
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
      return false;
    }
  } else {
    collectionName = suffixes[0];
    key = suffixes[1];
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    return false;
  }

  VPackSlice body = parsedBody->slice();

  if ((!isArrayCase && !body.isObject()) || (isArrayCase && !body.isArray())) {
    generateTransactionError(collectionName,
        OperationResult(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID), "");
    return false;
  }

  OperationOptions opOptions;
  opOptions.isRestore =
      extractBooleanParameter(StaticStrings::IsRestoreString, false);
  opOptions.ignoreRevs =
      extractBooleanParameter(StaticStrings::IgnoreRevsString, true);
  opOptions.waitForSync =
      extractBooleanParameter(StaticStrings::WaitForSyncString, false);
  opOptions.returnNew =
      extractBooleanParameter(StaticStrings::ReturnNewString, false);
  opOptions.returnOld =
      extractBooleanParameter(StaticStrings::ReturnOldString, false);
  opOptions.silent =
      extractBooleanParameter(StaticStrings::SilentString, false);
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);

  // extract the revision, if single document variant and header given:
  std::shared_ptr<VPackBuilder> builder;
  if (!isArrayCase) {
    TRI_voc_rid_t revision = 0;
    bool isValidRevision;
    revision = extractRevision("if-match", isValidRevision);
    if (!isValidRevision) {
      revision = UINT64_MAX;  // an impossible revision, so precondition failed
    }
    VPackSlice keyInBody = body.get(StaticStrings::KeyString);
    if ((revision != 0 && TRI_ExtractRevisionId(body) != revision) ||
        keyInBody.isNone() || keyInBody.isNull() ||
        (keyInBody.isString() && keyInBody.copyString() != key)) {
      // We need to rewrite the document with the given revision and key:
      builder = std::make_shared<VPackBuilder>();
      {
        VPackObjectBuilder guard(builder.get());
        TRI_SanitizeObject(body, *builder);
        builder->add(StaticStrings::KeyString, VPackValue(key));
        if (revision != 0) {
          builder->add(StaticStrings::RevString,
                       VPackValue(TRI_RidToString(revision)));
        }
      }
      body = builder->slice();
    }
    if (revision != 0) {
      opOptions.ignoreRevs = false;
    }
  }

  // find and load collection given by name or identifier
  auto transactionContext(transaction::StandaloneContext::Create(_vocbase));
  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::WRITE);
  if (!isArrayCase) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  // ...........................................................................
  // inside write transaction
  // ...........................................................................

  Result res = trx.begin();

  if (!res.ok()) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  OperationResult result(TRI_ERROR_NO_ERROR);
  if (isPatch) {
    // patching an existing document
    opOptions.keepNull =
        extractBooleanParameter(StaticStrings::KeepNullString, true);
    opOptions.mergeObjects =
        extractBooleanParameter(StaticStrings::MergeObjectsString, true);
    result = trx.update(collectionName, body, opOptions);
  } else {
    result = trx.replace(collectionName, body, opOptions);
  }

  res = trx.finish(result.code);

  // ...........................................................................
  // outside write transaction
  // ...........................................................................

  if (result.failed()) {
    generateTransactionError(result);
    return false;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, key, 0);
    return false;
  }

  generateSaved(result, collectionName,
                TRI_col_type_e(trx.getCollectionType(collectionName)),
                transactionContext->getVPackOptionsForDump(), isArrayCase);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_DELETE
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() < 1 || suffixes.size() > 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/document/<document-handle> or "
                  "/_api/document/<collection> with a BODY");
    return false;
  }

  // split the document reference
  std::string const& collectionName = suffixes[0];
  std::string key;
  if (suffixes.size() == 2) {
    key = suffixes[1];
  }

  // extract the revision if single document case
  TRI_voc_rid_t revision = 0;
  if (suffixes.size() == 2) {
    bool isValidRevision = false;
    revision = extractRevision("if-match", isValidRevision);
    if (!isValidRevision) {
      revision = UINT64_MAX;  // an impossible revision, so precondition failed
    }
  }

  OperationOptions opOptions;
  opOptions.returnOld =
      extractBooleanParameter(StaticStrings::ReturnOldString, false);
  opOptions.ignoreRevs =
      extractBooleanParameter(StaticStrings::IgnoreRevsString, true);
  opOptions.waitForSync =
      extractBooleanParameter(StaticStrings::WaitForSyncString, false);
  opOptions.silent =
      extractBooleanParameter(StaticStrings::SilentString, false);
  extractStringParameter(StaticStrings::IsSynchronousReplicationString,
                         opOptions.isSynchronousReplicationFrom);

  auto transactionContext(transaction::StandaloneContext::Create(_vocbase));

  VPackBuilder builder;
  VPackSlice search;
  std::shared_ptr<VPackBuilder> builderPtr;

  if (suffixes.size() == 2) {
    {
      VPackObjectBuilder guard(&builder);
      builder.add(StaticStrings::KeyString, VPackValue(key));
      if (revision != 0) {
        opOptions.ignoreRevs = false;
        builder.add(StaticStrings::RevString,
                    VPackValue(TRI_RidToString(revision)));
      }
    }
    search = builder.slice();
  } else {
    try {
      TRI_ASSERT(_request != nullptr);
      builderPtr = _request->toVelocyPackBuilderPtr();
    } catch (...) {
      // If an error occurs here the body is not parsable. Fail with bad
      // parameter
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "Request body not parseable");
      return false;
    }
    search = builderPtr->slice();
  }

  if (!search.isArray() && !search.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Request body not parseable");
    return false;
  }

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::WRITE);
  if (suffixes.size() == 2 || !search.isArray()) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();

  if (!res.ok()) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  OperationResult result = trx.remove(collectionName, search, opOptions);

  res = trx.finish(result.code);

  if (!result.successful()) {
    generateTransactionError(result);
    return false;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, key);
    return false;
  }

  generateDeleted(result, collectionName,
                  TRI_col_type_e(trx.getCollectionType(collectionName)),
                  transactionContext->getVPackOptionsForDump());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_READ_MANY
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readManyDocuments() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/document/<collection> with a BODY");
    return false;
  }

  // split the document reference
  std::string const& collectionName = suffixes[0];

  OperationOptions opOptions;
  opOptions.ignoreRevs =
      extractBooleanParameter(StaticStrings::IgnoreRevsString, true);

  auto transactionContext(transaction::StandaloneContext::Create(_vocbase));
  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::READ);

  // ...........................................................................
  // inside read transaction
  // ...........................................................................

  Result res = trx.begin();

  if (!res.ok()) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  TRI_ASSERT(_request != nullptr);
  VPackSlice search = _request->payload(transactionContext->getVPackOptions());

  OperationResult result = trx.document(collectionName, search, opOptions);

  res = trx.finish(result.code);

  if (!result.successful()) {
    generateTransactionError(result);
    return false;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  generateDocument(result.slice(), true,
                   transactionContext->getVPackOptionsForDump());
  return true;
}
