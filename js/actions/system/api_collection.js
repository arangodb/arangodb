////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing collections
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
var API = "_api/";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a collection
///
/// @REST{GET /_api/collection/@FA{collection-identifier}}
///
/// The result is an objects describing the collection with the following
/// attributes:
///
/// @FA{id}
///
/// The identifier of the collection.
///
/// @FA{name}
///
/// The name of the collection.
///
/// If the @FA{collection-identifier} is missing, then a @LIT{HTTP 400} is
/// returned.  If the @FA{collection-identifier} is unknown, then a @LIT{HTTP
/// 404} is returned. It is possible to specify a name instead of an identifier.
/// In this case the response will contain a field "Location" which contains the
/// correct location.
///
/// @EXAMPLES
///
/// Using an identifier:
///
/// @verbinclude api_database3
///
/// Using a name:
///
/// @verbinclude api_database2
////////////////////////////////////////////////////////////////////////////////

function GET_api_collection (req, res) {
  if (req.suffix.length != 1) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                "expect GET /" + API + "collection/<collection-identifer>")
  }
  else {
    var name = req.suffix[0];
    var id = parseInt(name) || name;
    var collection = db._collection(name);
    
    if (collection == null) {
      actions.collectionUnknown(req, res, name);
    }
    else {
      var result = {};
      
      result.id = collection._id;
      result.name = collection._name;
      
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads or creates a collection
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "collection",
  context : "api",

  callback : function (req, res) {
    if (req.requestType == actions.GET) {
      GET_api_collection(req, res);
    }
    /*
    else if (req.requestType == actions.POST) {
      POST_api_database_collection(req, res);
    }
    */
    else {
      actions.resultUnsupported(req, res);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
