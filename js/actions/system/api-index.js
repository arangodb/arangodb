////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing indexes
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
var API = "_api/index";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// @brief returns all indexes of an collection
///
/// @REST{GET /_api/index?collection=@FA{collection-identifier}}
///
/// Returns an object with an attribute @LIT{indexes} containing a list of all
/// index descriptions for the given collection. The same information is also
/// available in the @LIT{identifiers} as hash map with the index handle as
/// keys.
///
/// @EXAMPLES
///
/// Return information about all indexes:
///
/// @verbinclude api-index-all-indexes
////////////////////////////////////////////////////////////////////////////////

function GET_api_indexes (req, res) {
  var name = req.parameters.collection;
  var id = parseInt(name) || name;
  var collection = internal.db._collection(id);

  if (collection == null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var list = [];
  var ids = {};
  var indexes = collection.getIndexes();

  for (var i = 0;  i < indexes.length;  ++i) {
    var index = indexes[i];
    
    list.push(index);
    ids[index.id] = index;
  }

  var result = { indexes : list, identifiers : ids };
  
  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an index
///
/// @REST{GET /_api/index/@FA{index-handle}}
///
/// The result is an objects describing the index. It has at least the following
/// attributes:
///
/// - @LIT{id}: The identifier of the collection.
///
/// - @LIT{type}: The type of the collection.
///
/// All other attributes are type-dependent.
///
/// @EXAMPLES
///
/// @verbinclude api-index-primary-index
////////////////////////////////////////////////////////////////////////////////

function GET_api_index (req, res) {

  // .............................................................................
  // /_api/indexes?collection=<collection-identifier>
  // .............................................................................

  if (req.suffix.length == 0) {
    GET_api_indexes(req, res);
  }

  // .............................................................................
  // /_api/indexes/<collection-identifier>/<index-identifier>
  // .............................................................................

  else if (req.suffix.length == 2) {
    var name = decodeURIComponent(req.suffix[0]);
    var id = parseInt(name) || name;
    var collection = internal.db._collection(id);
    
    if (collection == null) {
      actions.collectionNotFound(req, res, name);
      return;
    }

    var iid = decodeURIComponent(req.suffix[1]);
    var index = collection.index(collection._id + "/" + iid);

    if (index == null) {
      actions.indexNotFound(req, res, collection, iid);
      return;
    }

    actions.resultOk(req, res, actions.HTTP_OK, index);
  }
  else {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "expect GET /" + API + "/<index-handle>");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo index
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_geo (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths: " + fields);
  }

  try {
    var index;

    if (fields.length == 1) {
      if (body.hasOwnProperty("geoJson")) {
        index = collection.ensureGeoIndex(fields[0], body.geoJson);
      }
      else {
        index = collection.ensureGeoIndex(fields[0]);
      }
    }
    else if (fields.length == 2) {
      index = collection.ensureGeoIndex(fields[0], fields[1]);
    }
    else {
      actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths of length 1 or 2: " + fields);
      return;
    }

    if (index.isNewlyCreated) {
      actions.resultOk(req, res, actions.HTTP_CREATED, index);
    }
    else {
      actions.resultOk(req, res, actions.HTTP_OK, index);
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash index
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_hash (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths: " + fields);
  }

  try {
    var index;

    if (body.unique) {
      index = collection.ensureUniqueConstraint.apply(collection, fields);
    }
    else {
      index = collection.ensureHashIndex.apply(collection, fields);
    }

    if (index.isNewlyCreated) {
      actions.resultOk(req, res, actions.HTTP_CREATED, index);
    }
    else {
      actions.resultOk(req, res, actions.HTTP_OK, index);
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skip-list
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_skiplist (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths: " + fields);
  }

  try {
    var index;

    if (body.unique) {
      index = collection.ensureUniqueSkiplist.apply(collection, fields);
    }
    else {
      index = collection.ensureSkiplist.apply(collection, fields);
    }

    if (index.isNewlyCreated) {
      actions.resultOk(req, res, actions.HTTP_CREATED, index);
    }
    else {
      actions.resultOk(req, res, actions.HTTP_OK, index);
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index
///
/// @REST{POST /_api/index?collection=@FA{collection-identifier}}
///
/// Creates a new index in the collection @FA{collection-identifier}. Expects
/// an object containing the index details.
///
/// See @ref IndexGeo, @ref IndexHash, and @ref IndexSkiplist for details.
///
/// If the index does not already exists and could be created, then a @LIT{HTTP
/// 201} is returned.  If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-identifier} is unknown, then a @LIT{HTTP 404} is
/// returned. It is possible to specify a name instead of an identifier.  
///
/// @EXAMPLES
///
/// Creating a geo index with a location attribute:
///
/// @verbinclude api-index-create-geo-location
///
/// Creating a geo index with latitude and longitude attributes:
///
/// @verbinclude api-index-create-geo-latitude-longitude
///
/// Creating an unique constraint:
///
/// @verbinclude api-index-create-new-unique-constraint
///
/// Creating a hash index:
///
/// @verbinclude api-index-create-new-hash-index
///
/// Creating a skip-list:
///
/// @verbinclude api-index-create-new-skiplist
///
/// Creating a unique skip-list:
///
/// @verbinclude api-index-create-new-unique-skiplist
////////////////////////////////////////////////////////////////////////////////

function POST_api_index (req, res) {
  if (req.suffix.length != 0) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "expect POST /" + API + "?collection=<collection-identifer>");
    return;
  }

  var name = req.parameters.collection;
  var id = parseInt(name) || name;
  var collection = internal.db._collection(id);

  if (collection == null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var body;

  try {
    body = JSON.parse(req.requestBody || "{}") || {};
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_HTTP_CORRUPTED_JSON, err);
    return;
  }

  if (body.type == "geo") {
    POST_api_index_geo(req, res, collection, body);
  }
  else if (body.type == "hash") {
    POST_api_index_hash(req, res, collection, body);
  }
  else if (body.type == "skiplist") {
    POST_api_index_skiplist(req, res, collection, body);
  }
  else {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "unknown index type '" + body.type + "'");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index
///
/// @REST{DELETE /_api/index/@FA{index-handle}}
///
/// Deletes an index with @FA{index-handle}.
///
/// @EXAMPLES
///
/// @verbinclude api-index-delete-unique-skiplist
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_index (req, res) {
  if (req.suffix.length != 2) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "expect DELETE /" + API + "/<index-handle>");
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var id = parseInt(name) || name;
  var collection = internal.db._collection(id);

  if (collection == null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  try {
    var iid = parseInt(decodeURIComponent(req.suffix[1]));
    var droped = collection.dropIndex(collection._id + "/" + iid);

    if (droped) {
      actions.resultOk(req, res, actions.HTTP_OK, { id : collection._id + "/" + iid });
    }
    else {
      actions.indexNotFound(req, res, collection, collection._id + "/" + iid);
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads or creates a collection
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,
  context : "api",

  callback : function (req, res) {
    if (req.requestType == actions.GET) {
      GET_api_index(req, res);
    }
    else if (req.requestType == actions.DELETE) {
      DELETE_api_index(req, res);
    }
    else if (req.requestType == actions.POST) {
      POST_api_index(req, res);
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
