////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestEdgeHandler.h"

#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "Rest/JsonContainer.h"
#include "VocBase/simple-collection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::avocado;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestEdgeHandler::RestEdgeHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestDocumentHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an edge
///
/// @REST{POST /edge?collection=@FA{collection-identifier}&from=@FA{from-handle}&to=@FA{to-handle}}
///
/// Creates a new edge in the collection identified by the
/// @FA{collection-identifier}.  A JSON representation of the document must be
/// passed as the body of the POST request. The object handle of the start point
/// must be passed in @FA{from-handle}. The object handle of the end point must
/// be passed in @FA{to-handle}.
///
/// In all other respects the method works like @LIT{POST /document}, see
/// @ref RestDocument for details.
///
/// If you request such an edge, the returned document will also contain the
/// attributes @LIT{_from} and @LIT{_to}.
///
/// @EXAMPLES
///
/// Create an edge:
///
/// @verbinclude rest_edge-create-edge
///
/// Read an edge:
///
/// @verbinclude rest_edge-read-edge
////////////////////////////////////////////////////////////////////////////////

bool RestEdgeHandler::createDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // edge
  TRI_sim_edge_t edge;

  // extract the from
  bool found;
  string from = request->value("from", found);

  if (! found || from.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'from' is missing, expecting " + EDGE_PATH + "?collection=<identifier>&from=<from-handle>&to=<to-handle>");
    return false;
  }

  // extract the to
  string to = request->value("to", found);

  if (! found || to.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'to' is missing, expecting " + EDGE_PATH + "?collection=<identifier>&from=<from-handle>&to=<to-handle>");
    return false;
  }

  // extract the cid
  string collection = request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_AVOCADO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // shall we create the collection?
  string createStr = request->value("createCollection", found);
  bool create = found ? StringUtils::boolean(createStr) : false;

  // auto-ptr that will free JSON data when scope is left
  JsonContainer container(TRI_UNKNOWN_MEM_ZONE, parseJsonBody());
  TRI_json_t* json = container.ptr();

  if (json == 0) {
    return false;
  }

  // find and load collection given by name or identifier
  int res = useCollection(collection, create);

  if (res != TRI_ERROR_NO_ERROR) {
    releaseCollection();
    return false;
  }

  // split document handle
  edge._fromCid = _collection->_cid;
  edge._toCid = _collection->_cid;

  res = parseDocumentId(from, edge._fromCid, edge._fromDid);

  if (res != TRI_ERROR_NO_ERROR) {
    releaseCollection();

    generateError(HttpResponse::BAD,
                  res,
                  "'from' is not a document handle");
    return false;
  }

  res = parseDocumentId(to, edge._toCid, edge._toDid);

  if (res != TRI_ERROR_NO_ERROR) {
    releaseCollection();

    generateError(HttpResponse::BAD,
                  res,
                  "'to' is not a document handle");
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  bool waitForSync = _documentCollection->base._waitForSync;
  TRI_voc_cid_t cid = _documentCollection->base._cid;

  // note: unlocked is performed by createJson()
  TRI_doc_mptr_t const mptr = _documentCollection->createJson(_documentCollection, TRI_DOC_MARKER_EDGE, json, &edge, true);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  // release collection and free json
  releaseCollection();

  // generate result
  if (mptr._did != 0) {
    if (waitForSync) {
      generateCreated(cid, mptr._did, mptr._rid);
    }
    else {
      generateAccepted(cid, mptr._did, mptr._rid);
    }

    return true;
  }
  else {
    int res = TRI_errno();

    switch (res) {
      case TRI_ERROR_AVOCADO_READ_ONLY:
        generateError(HttpResponse::FORBIDDEN, res, "collection is read-only");
        return false;

      case TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED:
        generateError(HttpResponse::CONFLICT, res, "cannot create document, unique constraint violated");
        return false;

      case TRI_ERROR_AVOCADO_GEO_INDEX_VIOLATED:
        generateError(HttpResponse::BAD, res, "geo constraint violated");
        return false;

      default:
        generateError(HttpResponse::SERVER_ERROR,
                      TRI_ERROR_INTERNAL,
                      "cannot create, failed with: " + string(TRI_last_error()));
        return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
