////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions modules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

var API = "_api/";
var ApiRequests = {};

ApiRequests.cursor = {};
ApiRequests.cursor["POST /"   + API + "cursor"]             = "create and execute query. (creates a cursor)";
ApiRequests.cursor["PUT /"    + API + "cursor/<cursor-id>"] = "get next results";
ApiRequests.cursor["DELETE /" + API + "cursor/<cursor-id>"] = "delete cursor";

ApiRequests.collection = {};
ApiRequests.collection["GET /" + API + "collections"]                = "get list of collections";
ApiRequests.collection["GET /" + API + "collection/<collection-id>"] = "get all elements of collection";

ApiRequests.document = {};
ApiRequests.document["POST /"   + API + "document/<collection-id>"]               = "create new document";
ApiRequests.document["PUT /"    + API + "document/<collection-id>/<document-id>"] = "update document";
ApiRequests.document["GET /"    + API + "document/<collection-id>/<document-id>"] = "get a document";       
ApiRequests.document["DELETE /" + API + "document/<collection-id>/<document-id>"] = "delete a document";

ApiRequests.query = {};
ApiRequests.query["POST /" + API + "query"] = "create a query";
ApiRequests.query["GET /" + API + "query/<query-id>"] = "get query";
ApiRequests.query["PUT /" + API + "query/<query-id>"] = "change query";
ApiRequests.query["DELETE /" + API + "query/<query-id>"] = "delete query";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a help
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "help",
  context : "api",

  callback : function (req, res) {
    var result = {
      requests : ApiRequests
    }
    
    actions.actionResultOK(req, res, 200, result);    
  }
});


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
