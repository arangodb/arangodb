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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBRestExportHandler.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RocksDBRestExportHandler::RocksDBRestExportHandler(application_features::ApplicationServer& server,
                                                   GeneralRequest* request,
                                                   GeneralResponse* response,
                                                   aql::QueryRegistry* queryRegistry)
    : RestCursorHandler(server, request, response, queryRegistry), _restrictions() {}

RestStatus RocksDBRestExportHandler::execute() {
  if (ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "'/_api/export' is not yet supported in a cluster");
    return RestStatus::DONE;
  }

  // extract the sub-request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    return createCursor();
  }

  if (type == rest::RequestType::PUT) {
    return RestCursorHandler::execute();
  }

  if (type == rest::RequestType::DELETE_REQ) {
    return RestCursorHandler::execute();
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

VPackBuilder RocksDBRestExportHandler::buildQueryOptions(std::string const& cname,
                                                         VPackSlice const& slice) {
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  VPackBuilder options;
  options.openObject();

  VPackSlice batchSize = slice.get("batchSize");
  if (batchSize.isNumber()) {
    if ((batchSize.isInteger() && batchSize.getUInt() == 0) ||
        (batchSize.isDouble() && batchSize.getDouble() == 0.0)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TYPE_ERROR, "expecting non-zero value for 'batchSize'");
    }
    options.add("batchSize", batchSize);
  } else {
    options.add("batchSize", VPackValue(1000));
  }

  VPackSlice ttl = slice.get("ttl");
  if (ttl.isNumber()) {
    options.add("ttl", ttl);
  } else {
    options.add("ttl", VPackValue(30));
  }

  int64_t limit = INT64_MAX;
  VPackSlice limitSlice = slice.get("limit");
  if (limitSlice.isNumber()) {
    limit = limitSlice.getInt();
  }

  options.add("options", VPackValue(VPackValueType::Object));
  options.add("stream", VPackValue(true));  // important!!
  VPackSlice count = slice.get("count");
  if (count.isBool() && count.getBool()) {
    // QueryStreamCursor will add `exportCount` as `count`
    options.add("exportCollection", VPackValue(cname));
  }
  options.close();  // options

  std::string query = "FOR doc IN @@collection ";

  options.add("bindVars", VPackValue(VPackValueType::Object));
  options.add("@collection", VPackValue(cname));
  if (limit != INT64_MAX) {
    query.append("LIMIT @limit ");
    options.add("limit", limitSlice);
  }

  // handle "restrict" parameter
  VPackSlice restrct = slice.get("restrict");
  if (restrct.isObject()) {
    // "restrict"."type"
    VPackSlice type = restrct.get("type");
    if (!type.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "expecting string for 'restrict.type'");
    }

    VPackSlice fields = restrct.get("fields");
    if (!fields.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "expecting array for 'restrict.fields'");
    }

    if (type.isEqualString("include")) {
      if (fields.length() == 0) {
        query.append("RETURN {}");
      } else {
        query.append("RETURN KEEP(doc");
      }
    } else if (type.isEqualString("exclude")) {
      if (fields.length() == 0) {
        query.append("RETURN doc");
      } else {
        query.append("RETURN UNSET(doc");
      }
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "expecting either 'include' or 'exclude' for 'restrict.type'");
    }

    if (fields.length() > 0) {
      // "restrict"."fields"
      int i = 0;
      for (auto const& name : VPackArrayIterator(fields)) {
        if (name.isString()) {
          std::string varName = std::string("var").append(std::to_string(i++));
          query.append(", @").append(varName);
          options.add(varName, VPackValue(name.copyString()));
        }
      }
      query += ")";
    }

  } else {
    if (!restrct.isNone()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TYPE_ERROR,
                                     "expecting object for 'restrict'");
    }
    query.append("RETURN doc");
  }

  options.close();  // bindVars
  options.add("query", VPackValue(query));
  options.close();

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_export
////////////////////////////////////////////////////////////////////////////////

RestStatus RocksDBRestExportHandler::createCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/export");
    return RestStatus::DONE;
  }

  // extract the cid
  bool found;
  std::string const& name = _request->value("collection", found);

  if (!found || name.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting "
                  "/_api/export?collection=<identifier>");
    return RestStatus::DONE;
  }

  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  VPackBuilder queryBody = buildQueryOptions(name, body);
  TRI_ASSERT(_query == nullptr);
  return registerQueryOrCursor(queryBody.slice());
}
