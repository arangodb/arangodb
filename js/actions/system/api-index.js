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

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

var API = "_api/index";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all indexes of an collection
///
/// @RESTHEADER{GET /_api/index,reads all indexes of a collection}
///
/// @REST{GET /_api/index?collection=@FA{collection-name}}
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
  var collection = arangodb.db._collection(name);
  
  if (collection === null) {
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
/// @RESTHEADER{GET /_api/index,reads an index}
///
/// @REST{GET /_api/index/@FA{index-handle}}
///
/// The result is an objects describing the index. It has at least the following
/// attributes:
///
/// - @LIT{id}: The identifier of the index.
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
  // /_api/index?collection=<collection-name>
  // .............................................................................

  if (req.suffix.length === 0) {
    GET_api_indexes(req, res);
  }

  // .............................................................................
  // /_api/index/<collection-name>/<index-identifier>
  // .............................................................................

  else if (req.suffix.length === 2) {
    var name = decodeURIComponent(req.suffix[0]);
    var collection = arangodb.db._collection(name);
    
    if (collection === null) {
      actions.collectionNotFound(req, res, name);
      return;
    }

    var iid = decodeURIComponent(req.suffix[1]);
    try {
      var index = collection.index(name + "/" + iid);
      if (index !== null) {
        actions.resultOk(req, res, actions.HTTP_OK, index);
        return;
      }
    }
    catch (err) {
      if (err.errorNum === arangodb.ERROR_ARANGO_INDEX_NOT_FOUND ||
          err.errorNum === arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        actions.indexNotFound(req, res, collection, iid);
        return;
      }
      throw err;
    }
  }
  else {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expect GET /" + API + "/<index-handle>");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cap constraint
///
/// @RESTHEADER{POST /_api/index,creates a cap constraint}
///
/// @REST{POST /_api/index?collection=@FA{collection-name}}
///
/// Creates a cap constraint (@ref IndexCapIntro) for the collection @FA{collection-name}, if
/// it does not already exist. Expects an object containing the index details.
///
/// - @LIT{type}: must be equal to @LIT{"cap"}.
///
/// - @LIT{size}: The maximal number of documents for the collection.
///
/// If the index does not already exist and could be created, then a @LIT{HTTP
/// 201} is returned. If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-name} is unknown, then a @LIT{HTTP 404} is returned.
///
/// @EXAMPLES
///
/// Creating a cap collection
///
/// @verbinclude api-index-create-new-cap-constraint
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_cap (req, res, collection, body) {
  if (! body.hasOwnProperty("size")) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expecting a size");
  }

  var size = body.size;
  var index = collection.ensureCapConstraint(size);

  if (index.isNewlyCreated) {
    actions.resultOk(req, res, actions.HTTP_CREATED, index);
  }
  else {
    actions.resultOk(req, res, actions.HTTP_OK, index);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo index
///
/// @RESTHEADER{POST /_api/index,creates a geo-spatial index}
///
/// @REST{POST /_api/index?collection=@FA{collection-name}}
///
/// Creates a geo-spatial index in the collection @FA{collection-name}, if
/// it does not already exist. Expects an object containing the index details.
///
/// - @LIT{type}: must be equal to @LIT{"geo"}.
///
/// - @LIT{fields}: A list with one or two attribute paths. <br>
///   <br>
///   If it is a list with one attribute path @FA{location}, then a geo-spatial
///   index on all documents is created using @FA{location} as path to the
///   coordinates. The value of the attribute must be a list with at least two
///   double values. The list must contain the latitude (first value) and the
///   longitude (second value). All documents, which do not have the attribute
///   path or with value that are not suitable, are ignored.<br>
///   <br>
///   If it is a list with two attribute paths @FA{latitude} and @FA{longitude},
///   then a geo-spatial index on all documents is created using @FA{latitude}
///   and @FA{longitude} as paths the latitude and the longitude. The value of
///   the attribute @FA{latitude} and of the attribute @FA{longitude} must a
///   double. All documents, which do not have the attribute paths or which
///   values are not suitable, are ignored.
///
/// - @LIT{geoJson}: If a geo-spatial index on a @FA{location} is constructed
///   and @LIT{geoJson} is @LIT{true}, then the order within the list is longitude
///   followed by latitude. This corresponds to the format described in <br>
///   <br>
///   http://geojson.org/geojson-spec.html#positions
///
/// - @LIT{constraint}: If @LIT{constraint} is @LIT{true}, then a geo-spatial
///   constraint instead of an index is created. 
///
/// - @LIT{ignoreNull}: If a geo-spatial constraint is created and
///   @FA{ignoreNull} is true, then documents with a null in @FA{location} or at
///   least one null in @FA{latitude} or @FA{longitude} are ignored.
///
/// If the index does not already exist and could be created, then a @LIT{HTTP
/// 201} is returned.  If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-name} is unknown, then a @LIT{HTTP 404} is returned.
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
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_geo (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths: " + fields);
  }

  var index;

  if (fields.length === 1) {

    // attribute list and geoJson
    if (body.hasOwnProperty("geoJson")) {
      if (body.hasOwnProperty("constraint") && body.constraint) {
        if (body.hasOwnProperty("ignoreNull")) {
          index = collection.ensureGeoConstraint(fields[0], body.geoJson, body.ignoreNull);
        }
        else {
          index = collection.ensureGeoConstraint(fields[0], body.geoJson, false);
        }
      }
      else {
        index = collection.ensureGeoIndex(fields[0], body.geoJson);
      }
    }

    // attribute list
    else {
      if (body.hasOwnProperty("constraint") && body.constraint) {
        if (body.hasOwnProperty("ignoreNull")) {
          index = collection.ensureGeoConstraint(fields[0], body.ignoreNull);
        }
        else {
          index = collection.ensureGeoConstraint(fields[0], false);
        }
      }
      else {
        index = collection.ensureGeoIndex(fields[0]);
      }
    }
  }

  // attributes
  else if (fields.length === 2) {
    if (body.hasOwnProperty("constraint") && body.constraint) {
      if (body.hasOwnProperty("ignoreNull")) {
        index = collection.ensureGeoConstraint(fields[0], fields[1], body.ignoreNull);
      }
      else {
        index = collection.ensureGeoConstraint(fields[0], fields[1], false);
      }
    }
    else {
      index = collection.ensureGeoIndex(fields[0], fields[1]);
    }
  }

  // something is wrong
  else {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
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

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash index
///
/// @RESTHEADER{POST /_api/index,creates a hash index}
///
/// @REST{POST /_api/index?collection=@FA{collection-name}}
///
/// Creates a hash index for the collection @FA{collection-name}, if it
/// does not already exist. The call expects an object containing the index
/// details.
///
/// - @LIT{type}: must be equal to @LIT{"hash"}.
///
/// - @LIT{fields}: A list of attribute paths.
///
/// - @LIT{unique}: If @LIT{true}, then create a unique index.
///
/// If the index does not already exist and could be created, then a @LIT{HTTP
/// 201} is returned.  If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-name} is unknown, then a @LIT{HTTP 404} is returned.
///
/// If the collection already contains documents and you try to create a unique
/// hash index in such a way that there are documents violating the uniqueness,
/// then a @LIT{HTTP 400} is returned.
///
/// @EXAMPLES
///
/// Creating an unique constraint:
///
/// @verbinclude api-index-create-new-unique-constraint
///
/// Creating a hash index:
///
/// @verbinclude api-index-create-new-hash-index
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_hash (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths: " + fields);
  }

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

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skip-list
///
/// @RESTHEADER{POST /_api/index,creates a hash index}
///
/// @REST{POST /_api/index?collection=@FA{collection-name}}
///
/// Creates a skip-list index for the collection @FA{collection-name}, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// - @LIT{type}: must be equal to @LIT{"skiplist"}.
///
/// - @LIT{fields}: A list of attribute paths.
///
/// - @LIT{unique}: If @LIT{true}, then create a unique index.
///
/// If the index does not already exist and could be created, then a @LIT{HTTP
/// 201} is returned.  If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-name} is unknown, then a @LIT{HTTP 404} is returned.
///
/// If the collection already contains documents and you try to create a unique
/// skip-list index in such a way that there are documents violating the
/// uniqueness, then a @LIT{HTTP 400} is returned.
///
/// @EXAMPLES
///
/// Creating a skiplist:
///
/// @verbinclude api-index-create-new-skiplist
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_skiplist (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths: " + fields);
  }

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

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a fulltext index
///
/// @RESTHEADER{POST /_api/index,creates a fulltext index}
///
/// @REST{POST /_api/index?collection=@FA{collection-name}}
///
/// Creates a fulltext index for the collection @FA{collection-name}, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// - @LIT{type}: must be equal to @LIT{"fulltext"}.
///
/// - @LIT{fields}: A list of attribute names. Currently, the list is limited 
///   to exactly one attribute, so the value of @LIT{fields} should look like
///   this for example: @LIT{[ "text" ]}.
///
/// - @LIT{minLength}: Minimum character length of words to index. Will default
///   to a server-defined value if unspecified. It is thus recommended to set
///   this value explicitly when creating the index.
///
/// If the index does not already exist and could be created, then a @LIT{HTTP
/// 201} is returned.  If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-name} is unknown, then a @LIT{HTTP 404} is returned.
///
/// @EXAMPLES
///
/// Creating a fulltext index:
///
/// @verbinclude api-index-create-new-fulltext
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_fulltext (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute names: " + fields);
  }

  if (fields.length != 1 || typeof fields[0] !== 'string') {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "fields must contain exactly one attribute name");
  }

  var index = collection.ensureFulltextIndex.call(collection, fields, body.minLength || undefined);

  if (index.isNewlyCreated) {
    actions.resultOk(req, res, actions.HTTP_CREATED, index);
  }
  else {
    actions.resultOk(req, res, actions.HTTP_OK, index);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a bitarray
///
/// @RESTHEADER{POST /_api/index,creates a bitarray index}
///
/// @REST{POST /_api/index?collection=@FA{collection-name}}
///
/// Creates a bitarray index for the collection @FA{collection-name}, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// - @LIT{type}: must be equal to @LIT{"bitarray"}.
///
/// - @LIT{fields}: A list of pairs. A pair consists of an attribute path followed by a list of values.
///
/// - @LIT{unique}: Must always be set to @LIT{false}.
///
/// If the index does not already exist and could be created, then a @LIT{HTTP
/// 201} is returned.  If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-name} is unknown, then a @LIT{HTTP 404} is returned.
///
/// @EXAMPLES
///
/// Creating a bitarray index:
///
/// @verbinclude api-index-create-new-bitarray
////////////////////////////////////////////////////////////////////////////////

function POST_api_index_bitarray (req, res, collection, body) {
  var fields = body.fields;

  if (! (fields instanceof Array)) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "fields must be a list of attribute paths: " + fields);
  }

  var index;

  if (body.unique) {
    throw "Bitarray indexes can not be unique";
  }
  else {
    index = collection.ensureBitarray.apply(collection, fields);
  }

  if (index.isNewlyCreated) {
    actions.resultOk(req, res, actions.HTTP_CREATED, index);
  }
  else {
    actions.resultOk(req, res, actions.HTTP_OK, index);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index
///
/// @RESTHEADER{POST /_api/index,creates an index}
///
/// @REST{POST /_api/index?collection=@FA{collection-name}}
///
/// Creates a new index in the collection @FA{collection-name}. Expects
/// an object containing the index details.
///
/// See @ref IndexCapHttp, @ref IndexGeoHttp, @ref IndexHashHttp, 
/// @ref IndexFulltextHttp, and @ref IndexSkiplistHttp for details. 
///
/// By default, non-unique indexes will be created. To change this, use the 
/// @LIT{unique} attribute in the index details and set its value to @LIT{true}.
///
/// If the index does not already exist and could be created, then a @LIT{HTTP
/// 201} is returned.  If the index already exists, then a @LIT{HTTP 200} is
/// returned.
///
/// If the @FA{collection-name} is unknown, then a @LIT{HTTP 404} is returned.
///
/// @EXAMPLES
///
/// Creating a unique constraint:
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
///
/// Creating a fulltext index:
///
/// @verbinclude api-index-create-new-fulltext
////////////////////////////////////////////////////////////////////////////////

function POST_api_index (req, res) {

  if (req.suffix.length !== 0) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "expect POST /" + API + "?collection=<collection-name>");
    return;
  }

  var name = req.parameters.collection;
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }
  
  if (body.type === "cap") {
    POST_api_index_cap(req, res, collection, body);
  }
  else if (body.type === "geo") {
    POST_api_index_geo(req, res, collection, body);
  }
  else if (body.type === "hash") {
    POST_api_index_hash(req, res, collection, body);
  }
  else if (body.type === "skiplist") {
    POST_api_index_skiplist(req, res, collection, body);
  }
  else if (body.type === "fulltext") {
    POST_api_index_fulltext(req, res, collection, body);
  }
  else if (body.type === "bitarray") {
    POST_api_index_bitarray(req, res, collection, body);
  }
  else {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "unknown index type '" + body.type + "'");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an index
///
/// @RESTHEADER{DELETE /_api/index,deletes an index}
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
  if (req.suffix.length !== 2) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expect DELETE /" + API + "/<index-handle>");
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var iid = parseInt(decodeURIComponent(req.suffix[1]));
  var dropped = collection.dropIndex(name + "/" + iid);

  if (dropped) {
    actions.resultOk(req, res, actions.HTTP_OK, { id : name + "/" + iid });
  }
  else {
    actions.indexNotFound(req, res, collection, name + "/" + iid);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads or creates an index
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,
  context : "api",

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        GET_api_index(req, res);
      }
      else if (req.requestType === actions.DELETE) {
        DELETE_api_index(req, res);
      }
      else if (req.requestType === actions.POST) {
        POST_api_index(req, res);
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
