////////////////////////////////////////////////////////////////////////////////
/// @brief change request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestChangeHandler.h"

#include "Basics/json.h"
//#include "Basics/conversions.h"
#include "Basics/files.h"
//#include "Basics/logging.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"
//#include "Utils/CollectionGuard.h"
//#include "Utils/transactions.h"
//#include "VocBase/replication-applier.h"
//#include "VocBase/replication-dump.h"
//#include "VocBase/server.h"
//#include "VocBase/update-policy.h"
//#include "VocBase/index.h"
#include "Utils/Exception.h"
#include "Wal/LogfileManager.h"
//#include "Cluster/ClusterMethods.h"
//#include "Cluster/ClusterComm.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                       initialize static variables
// -----------------------------------------------------------------------------

const uint64_t RestChangeHandler::defaultChunkSize = 128 * 1024;

const uint64_t RestChangeHandler::maxChunkSize = 128 * 1024 * 1024;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestChangeHandler::RestChangeHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestChangeHandler::~RestChangeHandler () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_t RestChangeHandler::execute () {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/changes");
    return status_t(Handler::HANDLER_DONE);
  }

  if (_request->requestType() != HttpRequest::HTTP_REQUEST_PUT) {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return status_t(Handler::HANDLER_DONE);
  }

  try {
    std::unique_ptr<TRI_json_t> json(parseJsonBody());

    if (json.get() == nullptr) {
      // error message already set in parseJsonBody()
      return status_t(Handler::HANDLER_DONE);
    }
  
    if (json.get()->_type != TRI_JSON_OBJECT) {
      generateError(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
      return status_t(Handler::HANDLER_DONE);
    }

    std::vector<TRI_vocbase_col_t*> collections;

    auto collectionJson = TRI_LookupObjectJson(json.get(), "collections");
    if (! TRI_IsArrayJson(collectionJson) || TRI_LengthArrayJson(collectionJson) == 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "no collections specified in \"collections\"");
    }

    size_t const n = TRI_LengthArrayJson(collectionJson);

    for (size_t i = 0; i < n; ++i) {
      auto collection = static_cast<TRI_json_t const*>(TRI_AtVector(&collectionJson->_value._objects, i));

      if (! TRI_IsStringJson(collection)) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "expecting array of strings in \"collections\"");
      }

      std::string const collectionName(collection->_value._string.data, collection->_value._string.length - 1);

      TRI_vocbase_col_t* c = TRI_LookupCollectionByNameVocBase(_vocbase, collectionName.c_str());

      if (c == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
      }

      collections.emplace_back(c);
    }

    handleCommand(collections);
  }
  catch (triagens::arango::Exception const& ex) { 
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  }
  catch (...) {
    generateError(TRI_ERROR_INTERNAL);
  }
  
  return status_t(Handler::HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the chunk size
////////////////////////////////////////////////////////////////////////////////

uint64_t RestChangeHandler::determineChunkSize () const {
  // determine chunk size
  uint64_t chunkSize = defaultChunkSize;

  bool found;
  char const* value = _request->value("chunkSize", found);

  if (found) {
    // url parameter "chunkSize" was specified
    chunkSize = StringUtils::uint64(value);

    // don't allow overly big allocations
    if (chunkSize > maxChunkSize) {
      chunkSize = maxChunkSize;
    }
  }

  return chunkSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_changes
/// @RESTHEADER{GET /_api/changes, Returns changes}
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestChangeHandler::handleCommand (std::vector<TRI_vocbase_col_t*> const& collections) {
  // determine start and end tick
  triagens::wal::LogfileManagerState state = triagens::wal::LogfileManager::instance()->state();
  TRI_voc_tick_t tickStart = 0;

  bool found;
  char const* value = _request->value("from", found);
  if (found) {
    tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
