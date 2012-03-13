////////////////////////////////////////////////////////////////////////////////
/// @brief administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("actions");

// -----------------------------------------------------------------------------
// --SECTION--                                            administration actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ActionsAdmin
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about all collections
///
/// @REST{GET /_system/collections}
///
/// Returns information about all collections of the database. The returned
/// array contains the following entries.
///
/// - path: The server directory containing the database.
/// - collections : An associative array of all collections.
///
/// An entry of collections is again an associative array containing the
/// following entries.
///
/// - name: The name of the collection.
/// - status: The status of the collection. 1 = new born, 2 = unloaded,
///     3 = loaded, 4 = corrupted.
///
/// @verbinclude rest15
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_system/collections",
  context : "admin",

  callback : function (req, res) {
    var colls;
    var coll;
    var result;

    colls = db._collections();
    result = {
      path : db._path,
      collections : {}
    };

    for (var i = 0;  i < colls.length;  ++i) {
      coll = colls[i];

      result.collections[coll._name] = {
        id : coll._id,
        name : coll._name,
        status : coll.status(),
        figures : coll.figures()
      };
    }

    actions.actionResult(req, res, 200, result);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
///
/// @REST{GET /_system/collection/load?collection=@FA{identifier}}
///
/// Loads a collection into memory.
///
/// @verbinclude restX
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_system/collection/load",
  context : "admin",

  callback : function (req, res) {
    try {
      req.collection.load();

      actions.actionResult(req, res, 204);
    }
    catch (err) {
      actions.actionError(req, res, err);
    }
  },

  parameters : {
    collection : "collection-identifier"
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief information about a collection
///
/// @REST{GET /_system/collection/info?collection=@FA{identifier}}
///
/// Returns information about a collection
///
/// @verbinclude rest16
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_system/collection/info",
  context : "admin",

  callback : function (req, res) {
    try {
      result = {};
      result.id = req.collection._id;
      result.name = req.collection._name;
      result.status = req.collection.status();
      result.figures = req.collection.figures();

      actions.actionResult(req, res, 200, result);
    }
    catch (err) {
      actions.actionError(req, res, err);
    }
  },

  parameters : {
    collection : "collection-identifier"
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about all documents
///
/// @REST{GET /_system/documents}
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_system/documents",
  context : "admin",

  callback : function (req, res) {
    queryReferences(req, res, req.collection.all());
  },

  parameters : {
    collection : "collection-identifier",
    blocksize : "number",
    page : "number"
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about all indexes of a collection
///
/// @REST{GET /_system/collection/indexes?collection=@FA{identifier}}
///
/// Returns information about all indexes of a collection of the database.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_system/collection/indexes",
  context : "admin",

  callback : function (req, res) {
    try {
      result = {};
      result.name = req.collection._name;
      result.id = req.collection._id;
      result.indexes = req.collection.getIndexes();

      actions.actionResult(req, res, 200, result);
    }
    catch (err) {
      actions.actionError(req, res, err);
    }
  },

  parameters : {
    collection : "collection-identifier"
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information the server
///
/// @REST{GET /_system/status}
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_system/status",
  context : "admin",

  callback : function (req, res) {
    try {
      result = {};
      result.system = SYS_PROCESS_STAT();

      actions.actionResult(req, res, 200, result);
    }
    catch (err) {
      actions.actionError(req, res, err);
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
