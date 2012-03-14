////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
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
var simple = require("simple-query");
var API = "_api/simple/";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_all
/// @brief returns all documents of a collection
///
/// @REST{PUT /_api/simple/all}
///
/// Returns all documents of a collections. The call expects a JSON
/// hash array as body with the following attributes:
///
/// @FA{collection}
///
/// The identifier or name of the collection to query.
///
/// @FA{skip} (optional)
///
/// The documents to skip in the query.
///
/// @FA{limit} (optional)
///
/// The maximal amount of documents to return.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "all",
  context : "api",

  callback : function (req, res) {
    var body = JSON.parse(req.requestBody || "{}");

    var limit = body.limit || null;
    var skip = body.skip || 0;
    var name = body.collection || null;

    if (req.requestType != actions.PUT) {
      actions.resultUnsupported(req, res);
    }
    else {
      collection = db._collection(name);

      if (collection == null) {
        actions.resultCollectionUnknown(req, res, name);
      }
      else {
        var result = collection.all().skip(skip).limit(limit);

        actions.actionResultOK(req, res, 200, result.toArray());
      }
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
