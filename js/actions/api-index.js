/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing indexes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

var API = "_api/index";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_index
/// @brief returns all indexes of a collection
///
/// @RESTHEADER{GET /_api/index, Read all indexes of a collection}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTDESCRIPTION
///
/// Returns an object with an attribute *indexes* containing an array of all
/// index descriptions for the given collection. The same information is also
/// available in the *identifiers* as an object with the index handles as
/// keys.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// returns a json object containing a list of indexes on that collection.
///
/// @EXAMPLES
///
/// Return information about all indexes:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexAllIndexes}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db[cn].ensureHashIndex("name");
///     db[cn].ensureSkiplist("price", { sparse: true });
///
///     var url = "/_api/index?collection=" + cn;
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_indexes (req, res) {
  var name = req.parameters.collection;
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var list = [], ids = {}, indexes = collection.getIndexes(), i;

  for (i = 0;  i < indexes.length;  ++i) {
    var index = indexes[i];

    list.push(index);
    ids[index.id] = index;
  }

  var result = { indexes : list, identifiers : ids };

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_reads_index
/// @brief returns an index
///
/// @RESTHEADER{GET /_api/index/{index-handle},Read index}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{index-handle,string,required}
/// The index-handle.
///
/// @RESTDESCRIPTION
///
/// The result is an object describing the index. It has at least the following
/// attributes:
///
/// - *id*: the identifier of the index
///
/// - *type*: the index type
///
/// All other attributes are type-dependent. For example, some indexes provide
/// *unique* or *sparse* flags, whereas others don't. Some indexes also provide 
/// a selectivity estimate in the *selectivityEstimate* attribute of the result.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index exists, then a *HTTP 200* is returned.
///
/// @RESTRETURNCODE{404}
/// If the index does not exist, then a *HTTP 404*
/// is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexPrimaryIndex}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index/" + cn + "/0";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_index (req, res) {

  // .............................................................................
  // /_api/index?collection=<collection-name>
  // .............................................................................

  if (req.suffix.length === 0) {
    get_api_indexes(req, res);
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
/// @startDocuBlock JSF_post_api_index_cap
/// @brief creates a cap constraint
///
/// @RESTHEADER{POST /_api/index, Create cap constraint}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{type,string,required,string}
/// must be equal to *"cap"*.
///
/// @RESTBODYPARAM{size,integer,optional,int64}
/// The maximal number of documents for the collection. If specified,
/// the value must be greater than zero.
///
/// @RESTBODYPARAM{byteSize,integer,optional,int64}
/// The maximal size of the active document data in the collection
/// (in bytes). If specified, the value must be at least 16384.
///
///
/// @RESTDESCRIPTION
///
/// Creates a cap constraint for the collection *collection-name*,
/// if it does not already exist. Expects an object containing the index details.
///
/// **Note**: The cap constraint does not index particular attributes of the
/// documents in a collection, but limits the number of documents in the
/// collection to a maximum value. The cap constraint thus does not support
/// attribute names specified in the *fields* attribute nor uniqueness of
/// any kind via the *unique* attribute.
///
/// It is allowed to specify either *size* or *byteSize*, or both at
/// the same time. If both are specified, then the automatic document removal
/// will be triggered by the first non-met constraint.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then an *HTTP 200* is returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then an *HTTP 201*
/// is returned.
///
/// @RESTRETURNCODE{400}
/// If either *size* or *byteSize* contain invalid values, then an *HTTP 400*
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Creating a cap constraint
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewCapConstraint}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = {
///       type: "cap",
///       size : 10
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_index_geo
/// @brief creates a geo index
///
/// @RESTHEADER{POST /_api/index, Create geo-spatial index}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// 
/// @RESTBODYPARAM{type,string,required,string}
/// must be equal to *"geo"*.
///
/// @RESTBODYPARAM{fields,array,required,string}
/// An array with one or two attribute paths.
///
/// If it is an array with one attribute path *location*, then a geo-spatial
/// index on all documents is created using *location* as path to the
/// coordinates. The value of the attribute must be an array with at least two
/// double values. The array must contain the latitude (first value) and the
/// longitude (second value). All documents, which do not have the attribute
/// path or with value that are not suitable, are ignored.
///
/// If it is an array with two attribute paths *latitude* and *longitude*,
/// then a geo-spatial index on all documents is created using *latitude*
/// and *longitude* as paths the latitude and the longitude. The value of
/// the attribute *latitude* and of the attribute *longitude* must a
/// double. All documents, which do not have the attribute paths or which
/// values are not suitable, are ignored.
///
/// @RESTBODYPARAM{geoJson,string,required,string}
/// If a geo-spatial index on a *location* is constructed
/// and *geoJson* is *true*, then the order within the array is longitude
/// followed by latitude. This corresponds to the format described in
/// http://geojson.org/geojson-spec.html#positions
///
/// @RESTDESCRIPTION
///
/// Creates a geo-spatial index in the collection *collection-name*, if
/// it does not already exist. Expects an object containing the index details.
///
/// Geo indexes are always sparse, meaning that documents that do not contain
/// the index attributes or have non-numeric values in the index attributes
/// will not be indexed.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a *HTTP 200* is returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a *HTTP 201*
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Creating a geo index with a location attribute:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateGeoLocation}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "geo", 
///       fields : [ "b" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Creating a geo index with latitude and longitude attributes:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateGeoLatitudeLongitude}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "geo", 
///       fields: [ "e", "f" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_index_hash
/// @brief creates a hash index
///
/// @RESTHEADER{POST /_api/index, Create hash index}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{type,string,required,string}
/// must be equal to *"hash"*.
///
/// @RESTBODYPARAM{fields,array,required,string}
/// an array of attribute paths.
///
/// @RESTBODYPARAM{unique,boolean,required,}
/// if *true*, then create a unique index.
///
/// @RESTBODYPARAM{sparse,boolean,required,}
/// if *true*, then create a sparse index.
///
/// @RESTDESCRIPTION
///
/// Creates a hash index for the collection *collection-name* if it
/// does not already exist. The call expects an object containing the index
/// details.
///
/// In a sparse index all documents will be excluded from the index that do not 
/// contain at least one of the specified index attributes (i.e. *fields*) or that 
/// have a value of *null* in any of the specified index attributes. Such documents 
/// will not be indexed, and not be taken into account for uniqueness checks if
/// the *unique* flag is set.
///
/// In a non-sparse index, these documents will be indexed (for non-present
/// indexed attributes, a value of *null* will be used) and will be taken into
/// account for uniqueness checks if the *unique* flag is set.
///
/// **Note**: unique indexes on non-shard keys are not supported in a cluster.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a *HTTP 200* is returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a *HTTP 201*
/// is returned.
///
/// @RESTRETURNCODE{400}
/// If the collection already contains documents and you try to create a unique
/// hash index in such a way that there are documents violating the uniqueness,
/// then a *HTTP 400* is returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Creating an unique constraint:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewUniqueConstraint}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "hash", 
///       unique: true, 
///       fields : [ "a", "b" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Creating a non-unique hash index:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewHashIndex}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "hash", 
///       unique: false, 
///       fields: [ "a", "b" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Creating a sparse index:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateSparseHashIndex}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "hash", 
///       unique: false, 
///       sparse: true, 
///       fields: [ "a" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_index_skiplist
/// @brief creates a skip-list
///
/// @RESTHEADER{POST /_api/index, Create skip list}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// 
/// @RESTBODYPARAM{type,string,required,string}
/// must be equal to *"skiplist"*.
///
/// @RESTBODYPARAM{fields,array,required,string}
/// an array of attribute paths.
///
/// @RESTBODYPARAM{unique,boolean,required,}
/// if *true*, then create a unique index.
///
/// @RESTBODYPARAM{sparse,boolean,required,}
/// if *true*, then create a sparse index.
///
/// @RESTDESCRIPTION
///
/// Creates a skip-list index for the collection *collection-name*, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// In a sparse index all documents will be excluded from the index that do not 
/// contain at least one of the specified index attributes (i.e. *fields*) or that 
/// have a value of *null* in any of the specified index attributes. Such documents 
/// will not be indexed, and not be taken into account for uniqueness checks if
/// the *unique* flag is set.
///
/// In a non-sparse index, these documents will be indexed (for non-present
/// indexed attributes, a value of *null* will be used) and will be taken into
/// account for uniqueness checks if the *unique* flag is set.
///
/// **Note**: unique indexes on non-shard keys are not supported in a cluster.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a *HTTP 200* is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a *HTTP 201*
/// is returned.
///
/// @RESTRETURNCODE{400}
/// If the collection already contains documents and you try to create a unique
/// skip-list index in such a way that there are documents violating the
/// uniqueness, then a *HTTP 400* is returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Creating a skiplist index:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewSkiplist}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "skiplist", 
///       unique: false, 
///       fields: [ "a", "b" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Creating a sparse skiplist index:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateSparseSkiplist}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "skiplist", 
///       unique: false, 
///       sparse: true, 
///       fields: [ "a" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_index_fulltext
/// @brief creates a fulltext index
///
/// @RESTHEADER{POST /_api/index, Create fulltext index}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection-name,string,required}
/// The collection name.
///
/// @RESTBODYPARAM{type,string,required,string}
/// must be equal to *"fulltext"*.
///
/// @RESTBODYPARAM{fields,array,required,string}
/// an array of attribute names. Currently, the array is limited
/// to exactly one attribute.
///
/// @RESTBODYPARAM{minLength,integer,required,int64}
/// Minimum character length of words to index. Will default
/// to a server-defined value if unspecified. It is thus recommended to set
/// this value explicitly when creating the index.
///
/// @RESTDESCRIPTION
///
/// Creates a fulltext index for the collection *collection-name*, if
/// it does not already exist. The call expects an object containing the index
/// details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then a *HTTP 200* is
/// returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then a *HTTP 201*
/// is returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Creating a fulltext index:
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewFulltext}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index?collection=" + cn;
///     var body = { 
///       type: "fulltext", 
///       fields: [ "text" ] 
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_index
/// @brief creates an index
///
/// @RESTHEADER{POST /_api/index, Create index}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTLBODYPARAM{index-details,json,required}
///
/// @RESTDESCRIPTION
///
/// Creates a new index in the collection *collection*. Expects
/// an object containing the index details.
///
/// The type of the index to be created must specified in the *type*
/// attribute of the index details. Depending on the index type, additional
/// other attributes may need to specified in the request in order to create
/// the index.
///
/// Most indexes (a notable exception being the cap constraint) require the
/// array of attributes to be indexed in the *fields* attribute of the index
/// details. Depending on the index type, a single attribute or multiple
/// attributes can be indexed.
///
/// Indexing system attributes such as *_id*, *_key*, *_from*, and *_to*
/// is not supported for user-defined indexes. Manually creating an index using
/// any of these attributes will fail with an error.
///
/// Some indexes can be created as unique or non-unique variants. Uniqueness
/// can be controlled for most indexes by specifying the *unique* flag in the
/// index details. Setting it to *true* will create a unique index.
/// Setting it to *false* or omitting the *unique* attribute will
/// create a non-unique index.
///
/// **Note**: The following index types do not support uniqueness, and using
/// the *unique* attribute with these types may lead to an error:
///
/// - cap constraints
/// - fulltext indexes
///
/// **Note**: Unique indexes on non-shard keys are not supported in a
/// cluster.
///
/// Hash and skiplist indexes can optionally be created in a sparse
/// variant. A sparse index will be created if the *sparse* attribute in
/// the index details is set to *true*. Sparse indexes do not index documents
/// for which any of the index attributes is either not set or is *null*. 
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index already exists, then an *HTTP 200* is returned.
///
/// @RESTRETURNCODE{201}
/// If the index does not already exist and could be created, then an *HTTP 201*
/// is returned.
///
/// @RESTRETURNCODE{400}
/// If an invalid index description is posted or attributes are used that the
/// target index will not support, then an *HTTP 400* is returned.
///
/// @RESTRETURNCODE{404}
/// If *collection* is unknown, then an *HTTP 404* is returned.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function post_api_index (req, res) {
  if (req.suffix.length !== 0) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
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

  // inject collection name into body
  if (body.collection === undefined) {
    body.collection = name;
  }

  // create the index
  var index = collection.ensureIndex(body);
  actions.resultOk(req, res, index.isNewlyCreated ? actions.HTTP_CREATED : actions.HTTP_OK, index);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_index_delete
/// @brief deletes an index
///
/// @RESTHEADER{DELETE /_api/index/{index-handle}, Delete index}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{index-handle,string,required}
/// The index handle.
///
/// @RESTDESCRIPTION
///
/// Deletes an index with *index-handle*.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the index could be deleted, then an *HTTP 200* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *index-handle* is unknown, then an *HTTP 404* is returned.
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestIndexDeleteUniqueSkiplist}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var url = "/_api/index/" + db.products.ensureSkiplist("a","b").id;
///
///     var response = logCurlRequest('DELETE', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function delete_api_index (req, res) {
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

  var iid = parseInt(decodeURIComponent(req.suffix[1]), 10);
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

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_index(req, res);
      }
      else if (req.requestType === actions.DELETE) {
        delete_api_index(req, res);
      }
      else if (req.requestType === actions.POST) {
        post_api_index(req, res);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
