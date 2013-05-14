/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true,
         stupid: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing structures
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

var API = "_api/structures";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection
///
/// @RESTHEADER{POST /_api/structure,creates a structured document}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// Creates a new document in the collection `collection`.
///
/// @RESTBODYPARAM{structure,json,required}
/// The structure definition.
///
/// @RESTDESCRIPTION
///
/// Creates a new document in the collection identified by the
/// `collection-identifier`.  A JSON representation of the document must be
/// passed as the body of the POST request. The document must fullfill the
/// requirements of the structure definition, see @ref ArangoStructures.
///
/// In all other respects the function is identical to the 
/// @ref triagens::arango::RestDocumentHandler::createDocument "POST /_api/structure".
////////////////////////////////////////////////////////////////////////////////

function post_api_structure (req, res) {
  var body;
  var collection;
  var id;
  var name;
  var result;
  var vertex;

  body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  name = decodeURIComponent(req.suffix[0]);
  id = parseInt(name,10) || name;
  collection = arangodb.db._structure(id);
  
  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  res = collection.save(body);

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a structure request
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,
  context : "api",

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_structure(req, res);
      }
      else if (req.requestType === actions.DELETE) {
        delete_api_structure(req, res);
      }
      else if (req.requestType === actions.POST) {
        post_api_structure(req, res);
      }
      else if (req.requestType === actions.PUT) {
        put_api_structure(req, res);
      }
      else {
        actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
