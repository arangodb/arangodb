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
/// @brief returns all indexes of a collection
///
/// @RESTHEADER{GET /_api/index,reads all indexes of a collection}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTDESCRIPTION
///
/// Returns an object with an attribute `indexes` containing a list of all
/// index descriptions for the given collection. The same information is also
/// available in the `identifiers` as hash map with the index handle as
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
/// @RESTHEADER{GET /_api/index/{index-handle},reads an index}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{index-handle,string,required}
/// The index-handle.
///
/// @RESTDESCRIPTION
///
/// The result is an objects describing the index. It has at least the following
/// attributes:
///
/// - `id`: The identifier of the index.
///
/// - `type`: The type of the collection.
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
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{cap-constraint,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a cap constraint (@ref IndexCapIntro) for the collection `collection-name`, if
/// it does not already exist. Expects an object containing the index details.
///
/// - `type`: must be equal to `"cap"`.
///
/// - `size`: The maximal number of documents for the collection.
///
/// Note that the cap constraint does not index particular attributes of the
/// documents in a collection, but limits the number of documents in the
/// collection to a maximum value. The cap constraint thus does not support
/// attribute names specified in the `fields` attribute nor uniqueness of
/// any kind via the `unique` attribute.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a `HTTP 200` is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a `HTTP 201`
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the `collection-name` is unknown, then a `HTTP 404` is returned.
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
    return;
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
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{index-details,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a geo-spatial index in the collection `collection-name`, if
/// it does not already exist. Expects an object containing the index details.
///
/// - `type`: must be equal to `"geo"`.
///
/// - `fields`: A list with one or two attribute paths. 
///
///   If it is a list with one attribute path `location`, then a geo-spatial
///   index on all documents is created using `location` as path to the
///   coordinates. The value of the attribute must be a list with at least two
///   double values. The list must contain the latitude (first value) and the
///   longitude (second value). All documents, which do not have the attribute
///   path or with value that are not suitable, are ignored.
///
///   If it is a list with two attribute paths `latitude` and `longitude`,
///   then a geo-spatial index on all documents is created using `latitude`
///   and `longitude` as paths the latitude and the longitude. The value of
///   the attribute `latitude` and of the attribute `longitude` must a
///   double. All documents, which do not have the attribute paths or which
///   values are not suitable, are ignored.
///
/// - `geoJson`: If a geo-spatial index on a `location` is constructed
///   and `geoJson` is `true`, then the order within the list is longitude
///   followed by latitude. This corresponds to the format described in 
///   http://geojson.org/geojson-spec.html#positions
///
/// - `constraint`: If `constraint` is `true`, then a geo-spatial
///   constraint is created. The constraint is a non-unique variant of the index. 
///   Note that it is also possible to set the `unique` attribute instead of 
///   the `constraint` attribute.
///
/// - `ignoreNull`: If a geo-spatial constraint is created and
///   `ignoreNull` is true, then documents with a null in `location` or at
///   least one null in `latitude` or `longitude` are ignored.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a `HTTP 200` is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a `HTTP 201`
/// is returned.  
///
/// @RESTRETURNCODE{404}
/// If the `collection-name` is unknown, then a `HTTP 404` is returned.
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
    return;
  }

  var index;
  var constraint;

  if (body.hasOwnProperty("unique")) {
    // try "unique" first
    constraint = body.unique;
  }
  else if (body.hasOwnProperty("constraint")) {
    // "constraint" next
    constraint = body.constraint;
  }
  else {
    constraint = false;
  }

  if (fields.length === 1) {

    // attribute list and geoJson
    if (body.hasOwnProperty("geoJson")) {
      if (constraint) {
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
      if (constraint) {
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
    if (constraint) {
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
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{index-details,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a hash index for the collection `collection-name`, if it
/// does not already exist. The call expects an object containing the index
/// details.
///
/// - `type`: must be equal to `"hash"`.
///
/// - `fields`: A list of attribute paths.
///
/// - `unique`: If `true`, then create a unique index.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a `HTTP 200` is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a `HTTP 201`
/// is returned.  
///
/// @RESTRETURNCODE{400}
/// If the collection already contains documents and you try to create a unique
/// hash index in such a way that there are documents violating the uniqueness,
/// then a `HTTP 400` is returned.
///
/// @RESTRETURNCODE{404}
/// If the `collection-name` is unknown, then a `HTTP 404` is returned.
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
                      "fields must be a list of attribute names: " + fields);
    return;
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
/// @RESTHEADER{POST /_api/index,creates a skip list}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{index-details,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a skip-list index for the collection `collection-name`, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// - `type`: must be equal to `"skiplist"`.
///
/// - `fields`: A list of attribute paths.
///
/// - `unique`: If `true`, then create a unique index.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a `HTTP 200` is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a `HTTP 201`
/// is returned.
///
/// @RESTRETURNCODE{400}
/// If the collection already contains documents and you try to create a unique
/// skip-list index in such a way that there are documents violating the
/// uniqueness, then a `HTTP 400` is returned.
///
/// @RESTRETURNCODE{404}
/// If the `collection-name` is unknown, then a `HTTP 404` is returned.
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
                      "fields must be a list of attribute names: " + fields);
    return;
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
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{index-details,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a fulltext index for the collection `collection-name`, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// - `type`: must be equal to `"fulltext"`.
///
/// - `fields`: A list of attribute names. Currently, the list is limited 
///   to exactly one attribute, so the value of `fields` should look like
///   this for example: `[ "text" ]`.
///
/// - `minLength`: Minimum character length of words to index. Will default
///   to a server-defined value if unspecified. It is thus recommended to set
///   this value explicitly when creating the index.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a `HTTP 200` is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a `HTTP 201`
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the `collection-name` is unknown, then a `HTTP 404` is returned.
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
    return;
  }

  if (fields.length != 1 || typeof fields[0] !== 'string') {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "fields must contain exactly one attribute name");
    return;
  }

  if (body.hasOwnProperty("unique") && body.unique) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "unique fulltext indexes are not supported");
    return;
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
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{index-details,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a bitarray index for the collection `collection-name`, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// - `type`: must be equal to `"bitarray"`.
///
/// - `fields`: A list of pairs. A pair consists of an attribute path followed by a list of values.
///
/// - `unique`: Must always be set to `false`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a `HTTP 200` is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a `HTTP 201`
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the `collection-name` is unknown, then a `HTTP 404` is returned.
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
    return;
  }

  var index;

  if (body.unique) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "Bitarray indexes can not be unique");
    return;
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
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{index-details,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a new index in the collection `collection-name`. Expects
/// an object containing the index details.
///
/// The type of the index to be created must specified in the `type`
/// attribute of the index details. Depending on the index type, additional
/// other attributes may need to specified in the request in order to create
/// the index.
///
/// See @ref IndexCapHttp, @ref IndexGeoHttp, @ref IndexHashHttp, 
/// @ref IndexFulltextHttp, and @ref IndexSkiplistHttp for details. 
/// 
/// Most indexes (a notable exception being the cap constraint) require the
/// list of attributes to be indexed in the `fields` attribute of the index
/// details. Depending on the index type, a single attribute or multiple 
/// attributes may be indexed.
///
/// Some indexes can be created as unique or non-unique variants. Uniqueness
/// can be controlled for most indexes by specifying the `unique` in the
/// index details. Setting it to `true` will create a unique index. 
/// Setting it to `false` or omitting the `unique` attribute will
/// create a non-unique index.
///
/// Note that the following index types do not support uniqueness, and using 
/// the `unique` attribute with these types may lead to an error:
/// - cap constraints
/// - fulltext indexes
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a `HTTP 200` is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a `HTTP 201`
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the `collection-name` is unknown, then a `HTTP 404` is returned.
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
                      "expecting POST /" + API + "?collection=<collection-name>");
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
/// @RESTHEADER{DELETE /_api/index/{index-handle},deletes an index}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{index-handle,string,required}
/// The index handle.
///
/// @RESTDESCRIPTION
///
/// Deletes an index with `index-handle`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{202}
/// If the index could be deleted, then a `HTTP 202` is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the `index-handle` is unknown, then a `HTTP 404` is returned.
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
      actions.resultException(req, res, err, undefined, false);
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
