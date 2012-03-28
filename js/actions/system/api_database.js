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
var API = "_api/database/";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_GET_api_datebase_collections
/// @brief returns all collections
///
/// @REST{GET /_api/database/collections}
///
/// Returns all collections. The result is a list of objects with the following
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
/// @EXAMPLES
///
/// @verbinclude api_database1
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "collections",
  context : "api",

  callback : function (req, res) {
    if (req.requestType != actions.GET) {
      actions.resultUnsupported(req, res);
    }
    else {
      var collections = db._collections();
      var result = [];

      for (var i = 0;  i < collections.length;  ++i) {
        collection = collections[i];

        result.push({ id : collection._id, name : collection._name });
      }

      actions.result(req, res, actions.HTTP_OK, result);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about a collection
///
/// @REST{GET /_api/database/collection/@FA{collection-identifier}}
///
/// The result is an objects with the following attributes:
///
/// @FA{id}
///
/// The identifier of the collection.
///
/// @FA{name}
///
/// The name of the collection.
///
/// @EXAMPLES
///
/// Using a name:
///
/// @verbinclude api_database2
///
/// Using an identifier:
///
/// @verbinclude api_database3
////////////////////////////////////////////////////////////////////////////////

function GET_api_database_collection (req, res) {
  if (req.suffix.length != 1) {
    actions.collectionUnknown(req, res);
  }
  else {
    var name = req.suffix[0];
    var id = parseInt(name);
    
    if (id != NaN) {
      name = id;
    }
    
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
/// @brief creates a new collection
///
/// @REST{POST /_api/database/collection}
///
/// Creates a new collection. If the collection could be create, a @LIT{HTTP 200}
/// is returned. If the collection already exists, a @LIT{HTTP 409} is
/// returned.
///
/// The call expects a JSON hash array as body with the following
/// attributes:
///
/// @FA{name}
///
/// The name of the collection.
///
/// @FA{waitForSync} (optional, default true)
///
/// If @FA{waitForSync} is false, then creation of documents will not wait
/// for the synchronization to file.
///
/// In case of success, returns information about the created collection:
///
/// @FA{id}
///
/// The identifier of the collection.
///
/// @FA{name}
///
/// The name of the collection.
///
/// @EXAMPLES
///
/// Create a collection named test:
///
/// @verbinclude api_database4
///
/// Try it again:
///
/// @verbinclude api_database5
////////////////////////////////////////////////////////////////////////////////

function POST_api_database_collection (req, res) {
  var body = JSON.parse(req.requestBody || "{}");
  var name = body.name;
  var waitForSync = true;

  if (body.hasOwnProperty("waitForSync")) {
    waitForSync = body.waitForSync;
  }

  if (name == null) {
    badParameter(req, res, "name");
  }
  else {
    var collection = db._collection(name);

    if (collection != null) {
      actions.error(req, res, 
                    actions.HTTP_CONFLICT, 
                    actions.VERR_COLLECTION_EXISTS,
                    "collection already exists",
                    undefined,
                    { name : collection._name, id : collection._id });
    }
    else {
      collection = db[name];

      if (collection == null) {
        actions.badParameter(req, res, "cannot create collection named '" + name + "'");
      }
      else {
        if (collection._id == 0) {
          collection.load();
        }

        if (collection._id == 0) {
          actions.badParameter(req, res, "cannot create collection named '" + name + "'");
        }
        else {
          var result = {};

          result.id = collection._id;
          result.name = collection._name;

          collection.parameter({ waitForSync : waitForSync });

          actions.resultOk(req, res, actions.HTTP_OK, result);
        }
      }
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
      GET_api_database_collection(req, res);
    }
    else if (req.requestType == actions.POST) {
      POST_api_database_collection(req, res);
    }
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
