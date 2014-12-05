/*jshint strict: false */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing collections
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
var cluster = require("org/arangodb/cluster");

var API = "_api/collection";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a prefixed URL
////////////////////////////////////////////////////////////////////////////////

function databasePrefix (req, url) {
  if (req.hasOwnProperty('compatibility') && req.compatibility < 10400) {
    // pre 1.4-style location response (e.g. /_api/collection/xyz)
    return url;
  }

  // 1.4-style location response (e.g. /_db/dbname/_api/collection/xyz)
  return "/_db/" + encodeURIComponent(arangodb.db._name()) + url;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief collection representation
////////////////////////////////////////////////////////////////////////////////

function collectionRepresentation (collection, showProperties, showCount, showFigures) {
  var result = {};

  result.id = collection._id;
  result.name = collection.name();
  result.isSystem = (result.name.charAt(0) === '_');

  if (showProperties) {
    var properties = collection.properties();

    result.doCompact     = properties.doCompact;
    result.isVolatile    = properties.isVolatile;
    result.journalSize   = properties.journalSize;
    result.keyOptions    = properties.keyOptions;
    result.waitForSync   = properties.waitForSync;

    if (cluster.isCoordinator()) {
      result.shardKeys = properties.shardKeys;
      result.numberOfShards = properties.numberOfShards;
    }
  }

  if (showCount) {
    result.count = collection.count();
  }

  if (showFigures) {
    var figures = collection.figures();

    if (figures) {
      result.figures = figures;
    }
  }

  result.status = collection.status();
  result.type = collection.type();

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief helper to parse arguments for creating collections
////////////////////////////////////////////////////////////////////////////////

function parseBodyForCreateCollection (req, res) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return { bodyIsEmpty: true };
  }

  var r = {};

  if (! body.hasOwnProperty("name")) {
    r.name = "";
  }
  else {
    r.name = body.name;
  }
  r.parameter = { waitForSync : false };
  r.type = arangodb.ArangoCollection.TYPE_DOCUMENT;

  if (body.hasOwnProperty("doCompact")) {
    r.parameter.doCompact = body.doCompact;
  }

  if (body.hasOwnProperty("isSystem")) {
    r.parameter.isSystem = body.isSystem;
  }

  if (body.hasOwnProperty("isVolatile")) {
    r.parameter.isVolatile = body.isVolatile;
  }

  if (body.hasOwnProperty("journalSize")) {
    r.parameter.journalSize = body.journalSize;
  }

  if (body.hasOwnProperty("keyOptions")) {
    r.parameter.keyOptions = body.keyOptions;
  }

  if (body.hasOwnProperty("type")) {
    r.type = body.type;
  }

  if (body.hasOwnProperty("waitForSync")) {
    r.parameter.waitForSync = body.waitForSync;
  }

  if (body.hasOwnProperty("shardKeys") && cluster.isCoordinator()) {
    r.parameter.shardKeys = body.shardKeys || { };
  }

  if (body.hasOwnProperty("numberOfShards") && cluster.isCoordinator()) {
    r.parameter.numberOfShards = body.numberOfShards || 0;
  }

  if (body.hasOwnProperty("distributeShardsLike") && cluster.isCoordinator()) {
    r.parameter.distributeShardsLike = body.distributeShardsLike || "";
  }

  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_collection
/// @brief creates a collection
///
/// @RESTHEADER{POST /_api/collection, Create collection}
///
/// @RESTBODYPARAM{body,json,required}
/// the body with name and options for a collection.
///
/// @RESTDESCRIPTION
/// Creates an new collection with a given name. The request must contain an
/// object with the following attributes.
///
/// - *name*: The name of the collection.
///
/// - *waitForSync* (optional, default: false): If *true* then
///   the data is synchronised to disk before returning from a create or
///   update of a document.
///
/// - *doCompact* (optional, default is *true*): whether or not the collection
///   will be compacted.
///
/// - *journalSize* (optional, default is a configuration parameter): The 
///   maximal size of a journal or datafile in bytes. The value 
///   must be at least `1048576` (1 MB).
///
/// - *isSystem* (optional, default is *false*): If *true*, create a
///   system collection. In this case *collection-name* should start with
///   an underscore. End users should normally create non-system collections
///   only. API implementors may be required to create system collections in
///   very special occasions, but normally a regular collection will do.
///
/// - *isVolatile* (optional, default is *false*): If *true* then the
///   collection data is kept in-memory only and not made persistent. Unloading
///   the collection will cause the collection data to be discarded. Stopping
///   or re-starting the server will also cause full loss of data in the
///   collection. Setting this option will make the resulting collection be
///   slightly faster than regular collections because ArangoDB does not
///   enforce any synchronisation to disk and does not calculate any CRC
///   checksums for datafiles (as there are no datafiles). This option 
///   should threrefore be used for cache-type collections only, and not 
///   for data that cannot be re-created otherwise.
///
/// - *keyOptions* (optional) additional options for key generation. If
///   specified, then *keyOptions* should be a JSON array containing the
///   following attributes (note: some of them are optional):
///   - *type*: specifies the type of the key generator. The currently
///     available generators are *traditional* and *autoincrement*.
///   - *allowUserKeys*: if set to *true*, then it is allowed to supply
///     own key values in the *_key* attribute of a document. If set to
///     *false*, then the key generator will solely be responsible for
///     generating keys and supplying own key values in the *_key* attribute
///     of documents is considered an error.
///   - *increment*: increment value for *autoincrement* key generator.
///     Not used for other key generator types.
///   - *offset*: initial offset value for *autoincrement* key generator.
///     Not used for other key generator types.
///
/// - *type* (optional, default is *2*): the type of the collection to
///   create. The following values for *type* are valid:
///   - *2*: document collection
///   - *3*: edges collection
///
/// - *numberOfShards* (optional, default is *1*): in a cluster, this value
///   determines the number of shards to create for the collection. In a single
///   server setup, this option is meaningless.
///
/// - *shardKeys* (optional, default is *[ "_key" ]*): in a cluster, this
///   attribute determines which document attributes are used to determine the
///   target shard for documents. Documents are sent to shards based on the
///   values of their shard key attributes. The values of all shard
///   key attributes in a document are hashed, and the hash value is used to
///   determine the target shard.
/// **Note**: Values of shard key attributes cannot be changed once set.
///   This option is meaningless in a single server setup.
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionCreateCollection}
///     var url = "/_api/collection";
///     var body = {
///       name: "testCollectionBasics"
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     body = {
///       name: "testCollectionEdges",
///       type : 3
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///
///     assert(response.code === 200);
///     logJsonResponse(response);
///
///     db._flushCache();
///     db._drop("testCollectionBasics");
///     db._drop("testCollectionEdges");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionCreateKeyopt}
///     var url = "/_api/collection";
///     var body = {
///       name: "testCollectionUsers",
///       keyOptions : {
///         type : "autoincrement",
///         increment : 5,
///         allowUserKeys : true
///       }
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///
///     assert(response.code === 200);
///     logJsonResponse(response);
///
///     db._flushCache();
///     db._drop("testCollectionUsers");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function post_api_collection (req, res) {
  var r = parseBodyForCreateCollection(req, res);

  if (r.bodyIsEmpty) {
    return;   // error in JSON, is already reported
  }

  if (r.name === "") {
    actions.resultBad(req, res, arangodb.ERROR_ARANGO_ILLEGAL_NAME,
                      "name must be non-empty");
    return;
  }

  try {
    var collection;
    if (typeof(r.type) === "string") {
      if (r.type.toLowerCase() === "edge" || r.type === "3") {
        r.type = arangodb.ArangoCollection.TYPE_EDGE;
      }
    }
    if (r.type === arangodb.ArangoCollection.TYPE_EDGE) {
      collection = arangodb.db._createEdgeCollection(r.name, r.parameter);
    }
    else {
      collection = arangodb.db._createDocumentCollection(r.name, r.parameter);
    }

    var result = {};

    result.id = collection._id;
    result.name = collection.name();
    result.waitForSync = r.parameter.waitForSync || false;
    result.isVolatile = r.parameter.isVolatile || false;
    result.isSystem = r.parameter.isSystem || false;
    result.status = collection.status();
    result.type = collection.type();
    result.keyOptions = collection.keyOptions;

    if (cluster.isCoordinator()) {
      result.shardKeys = collection.shardKeys;
      result.numberOfShards = collection.numberOfShards;
      result.distributeShardsLike = collection.distributeShardsLike || "";
    }

    var headers = {
      location: databasePrefix(req, "/" + API + "/" + result.name)
    };

    actions.resultOk(req, res, actions.HTTP_OK, result, headers);
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_collections
/// @brief returns all collections
///
/// @RESTHEADER{GET /_api/collection,reads all collections}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{excludeSystem,boolean,optional}
/// Whether or not system collections should be excluded from the result.
///
/// @RESTDESCRIPTION
/// Returns an object with an attribute *collections* containing a
/// list of all collection descriptions. The same information is also
/// available in the *names* as hash map with the collection names
/// as keys.
///
/// By providing the optional URL parameter *excludeSystem* with a value of
/// *true*, all system collections will be excluded from the response.
///
/// @EXAMPLES
///
/// Return information about all collections:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetAllCollections}
///     var url = "/_api/collection";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_collections (req, res) {
  var i;
  var list = [];
  var names = {};
  var excludeSystem;
  var collections = arangodb.db._collections();

  excludeSystem = false;
  if (req.parameters.hasOwnProperty('excludeSystem')) {
    var value = req.parameters.excludeSystem.toLowerCase();
    if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
      excludeSystem = true;
    }
  }

  for (i = 0;  i < collections.length;  ++i) {
    var collection = collections[i];
    var rep = collectionRepresentation(collection);

    // include system collections or exclude them?
    if (! excludeSystem || rep.name.substr(0, 1) !== '_') {
      list.push(rep);
      names[rep.name] = rep;
    }
  }

  var result = { collections : list, names : names };

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_get_api_collection_name
/// @brief returns a collection
///
/// @RESTHEADER{GET /_api/collection/{collection-name}, Return information about a collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// The result is an object describing the collection with the following
/// attributes:
///
/// - *id*: The identifier of the collection.
///
/// - *name*: The name of the collection.
///
/// - *status*: The status of the collection as number.
///  - 1: new born collection
///  - 2: unloaded
///  - 3: loaded
///  - 4: in the process of being unloaded
///  - 5: deleted
///
/// Every other status indicates a corrupted collection.
///
/// - *type*: The type of the collection as number.
///   - 2: document collection (normal case)
///   - 3: edges collection
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is
/// returned.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_get_api_collection_properties
///
/// @RESTHEADER{GET /_api/collection/{collection-name}/properties, Read properties of a collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// In addition to the above, the result will always contain the
/// *waitForSync*, *doCompact*, *journalSize*, and *isVolatile* attributes.
/// This is achieved by forcing a load of the underlying collection.
///
/// - *waitForSync*: If *true* then creating or changing a
///   document will wait until the data has been synchronised to disk.
///
/// - *doCompact*: Whether or not the collection will be compacted.
///
/// - *journalSize*: The maximal size setting for journals / datafiles
///   in bytes.
///
/// - *isVolatile*: If *true* then the collection data will be
///   kept in memory only and ArangoDB will not write or sync the data
///   to disk.
///
/// In a cluster setup, the result will also contain the following attributes:
/// - *numberOfShards*: the number of shards of the collection.
///
/// - *shardKeys*: contains the names of document attributes that are used to
///   determine the target shard for documents.
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404*
/// is returned.
///
/// @EXAMPLES
///
/// Using an identifier:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionIdentifier}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn, { waitForSync: true });
///     var url = "/_api/collection/"+ coll._id + "/properties";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a name:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionName}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn, { waitForSync: true });
///     var url = "/_api/collection/products/properties";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_get_api_collection_count
///
/// @RESTHEADER{GET /_api/collection/{collection-name}/count, Return number of documents in a collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// In addition to the above, the result also contains the number of documents.
/// **Note** that this will always load the collection into memory.
///
/// - *count*: The number of documents inside the collection.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404*
/// is returned.
///
/// @EXAMPLES
///
/// Requesting the number of documents:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionCount}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn, { waitForSync: true });
///     for(var i=0;i<100;i++) {
///        coll.save({"count" :  i });
///     }
///     var url = "/_api/collection/"+ coll.name() + "/count";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_get_api_collection_figures
///
/// @RESTHEADER{GET /_api/collection/{collection-name}/figures, Return statistics for a collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// In addition to the above, the result also contains the number of documents
/// and additional statistical information about the collection.
/// **Note** : This will always load the collection into memory.
///
/// - *count*: The number of documents currently present in the collection.
///
/// * *figures.alive.count*: The number of curretly active documents in all datafiles
///   and journals of the collection. Documents that are contained in the
///   write-ahead log only are not reported in this figure.
///
/// * *figures.alive.size*: The total size in bytes used by all active documents of
///   the collection. Documents that are contained in the write-ahead log only are
///   not reported in this figure.
///
/// - *figures.dead.count*: The number of dead documents. This includes document
///   versions that have been deleted or replaced by a newer version. Documents
///   deleted or replaced that are contained the write-ahead log only are not reported
///   in this figure.
///
/// * *figures.dead.size*: The total size in bytes used by all dead documents.
///
/// * *figures.dead.deletion*: The total number of deletion markers. Deletion markers
///   only contained in the write-ahead log are not reporting in this figure.
///
/// - *figures.datafiles.count*: The number of datafiles.
/// - *figures.datafiles.fileSize*: The total filesize of datafiles (in bytes).
///
/// - *figures.journals.count*: The number of journal files.
/// - *figures.journals.fileSize*: The total filesize of all journal files (in bytes).
///
/// - *figures.compactors.count*: The number of compactor files.
/// - *figures.compactors.fileSize*: The total filesize of all compactor files (in bytes).
///
/// * *figures.shapefiles.count*: The number of shape files. This value is
///   deprecated and kept for compatibility reasons only. The value will always
///   be 0 since ArangoDB 2.0 and higher.
/// * *figures.shapefiles.fileSize*: The total filesize of the shape files. This
///   value is deprecated and kept for compatibility reasons only. The value will
///   always be 0 in ArangoDB 2.0 and higher.
///
/// * *figures.shapes.count*: The total number of shapes used in the collection.
///   This includes shapes that are not in use anymore. Shapes that are contained
///   in the write-ahead log only are not reported in this figure.
/// * *figures.shapes.size*: The total size of all shapes (in bytes). This includes
///   shapes that are not in use anymore. Shapes that are contained in the
///   write-ahead log only are not reported in this figure.
///
/// * *figures.attributes.count*: The total number of attributes used in the
///   collection. Note: the value includes data of attributes that are not in use
///   anymore. Attributes that are contained in the write-ahead log only are
///   not reported in this figure.
/// * *figures.attributes.size*: The total size of the attribute data (in bytes).
///   Note: the value includes data of attributes that are not in use anymore.
///   Attributes that are contained in the write-ahead log only are not
///   reported in this figure.
///
/// * *figures.indexes.count*: The total number of indexes defined for the
///   collection, including the pre-defined indexes (e.g. primary index).
///
/// * *figures.indexes.size*: The total memory allocated for indexes in bytes.
///
/// * *figures.maxTick*: The tick of the last marker that was stored in a journal
///   of the collection. This might be 0 if the collection does not yet have
///   a journal.
///
/// * *figures.uncollectedLogfileEntries*: The number of markers in the write-ahead
///   log for this collection that have not been transferred to journals or
///   datafiles.
///
/// - *journalSize*: The maximal size of a journal or datafile in bytes.
///
/// **Note**: collection data that are stored in the write-ahead log only are
/// not reported in the results. When the write-ahead log is collected, documents
/// might be added to journals and datafiles of the collection, which may modify
/// the figures of the collection.
///
/// Additionally, the filesizes of collection and index parameter JSON files are
/// not reported. These files should normally have a size of a few bytes
/// each. Please also note that the *fileSize* values are reported in bytes
/// and reflect the logical file sizes. Some filesystems may use optimisations
/// (e.g. sparse files) so that the actual physical file size is somewhat
/// different. Directories and sub-directories may also require space in the
/// file system, but this space is not reported in the *fileSize* results.
///
/// That means that the figures reported do not reflect the actual disk
/// usage of the collection with 100% accuracy. The actual disk usage of
/// a collection is normally slightly higher than the sum of the reported
/// *fileSize* values. Still the sum of the *fileSize* values can still be
/// used as a lower bound approximation of the disk usage.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404*
/// is returned.
///
/// @EXAMPLES
///
/// Using an identifier and requesting the figures of the collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionFigures}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn);
///     coll.save({"test":"hello"});
///     require("internal").wal.flush(true, true);
///     var url = "/_api/collection/"+ coll.name() + "/figures";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_get_api_collection_revision
///
/// @RESTHEADER{GET /_api/collection/{collection-name}/revision, Return collection revision id}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// In addition to the above, the result will also contain the
/// collection's revision id. The revision id is a server-generated
/// string that clients can use to check whether data in a collection
/// has changed since the last revision check.
///
/// - *revision*: The collection revision id as a string.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404*
/// is returned.
///
/// @EXAMPLES
///
/// Retrieving the revision of a collection
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionRevision}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn, { waitForSync: false });
///     var url = "/_api/collection/"+ coll.name() + "/revision";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_get_api_collection_checksum
///
/// @RESTHEADER{GET /_api/collection/{collection-name}/checksum, Return checksum for the collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{withRevisions,boolean,optional}
/// Whether or not to include document revision ids in the checksum calculation.
///
/// @RESTQUERYPARAM{withData,boolean,optional}
/// Whether or not to include document body data in the checksum calculation.
///
/// @RESTDESCRIPTION
/// Will calculate a checksum of the meta-data (keys and optionally revision ids) and
/// optionally the document data in the collection.
///
/// The checksum can be used to compare if two collections on different ArangoDB
/// instances contain the same contents. The current revision of the collection is
/// returned too so one can make sure the checksums are calculated for the same
/// state of data.
///
/// By default, the checksum will only be calculated on the *_key* system attribute
/// of the documents contained in the collection. For edge collections, the system
/// attributes *_from* and *_to* will also be included in the calculation.
///
/// By setting the optional URL parameter *withRevisions* to *true*, then revision
/// ids (*_rev* system attributes) are included in the checksumming.
///
/// By providing the optional URL parameter *withData* with a value of *true*,
/// the user-defined document attributes will be included in the calculation too.
/// **Note**: Including user-defined attributes will make the checksumming slower.
///
/// The response is a JSON object with the following attributes:
///
/// - *checksum*: The calculated checksum as a number.
///
/// - *revision*: The collection revision id as a string.
///
/// **Note**: this method is not available in a cluster.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404*
/// is returned.
///
/// @EXAMPLES
///
/// Retrieving the checksum of a collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionChecksum}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn);
///     coll.save({ foo: "bar" });
///     var url = "/_api/collection/" + coll.name() + "/checksum";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Retrieving the checksum of a collection including the collection data,
/// but not the revisions:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionChecksumNoRev}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn);
///     coll.save({ foo: "bar" });
///     var url = "/_api/collection/" + coll.name() + "/checksum?withRevisions=false&withData=true";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_collection (req, res) {
  var name;
  var result;
  var sub;

  // .............................................................................
  // /_api/collection
  // .............................................................................

  if (req.suffix.length === 0 && req.parameters.id === undefined) {
    get_api_collections(req, res);
    return;
  }

  // .............................................................................
  // /_api/collection/<name>
  // .............................................................................

  name = decodeURIComponent(req.suffix[0]);

  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var headers;

  // .............................................................................
  // /_api/collection/<name>
  // .............................................................................

  if (req.suffix.length === 1) {
    result = collectionRepresentation(collection, false, false, false);
    headers = {
      location : databasePrefix(req, "/" + API + "/" + collection.name())
    };
    actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    return;
  }

  if (req.suffix.length === 2) {
    sub = decodeURIComponent(req.suffix[1]);

    // .............................................................................
    // /_api/collection/<identifier>/checksum
    // .............................................................................

    if (sub === "checksum") {
      var withRevisions = false;
      var withData = false;
      var value;
      if (req.parameters.hasOwnProperty('withRevisions')) {
        value = req.parameters.withRevisions.toLowerCase();
        if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
          withRevisions = true;
        }
      }

      if (req.parameters.hasOwnProperty('withData')) {
        value = req.parameters.withData.toLowerCase();
        if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
          withData = true;
        }
      }

      result = collectionRepresentation(collection, false, false, false);
      var checksum = collection.checksum(withRevisions, withData);
      result.checksum = checksum.checksum;
      result.revision = checksum.revision;
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }

    // .............................................................................
    // /_api/collection/<identifier>/figures
    // .............................................................................

    else if (sub === "figures") {
      result = collectionRepresentation(collection, true, true, true);
      headers = {
        location : databasePrefix(req, "/" + API + "/" + collection.name() + "/figures")
      };
      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/count
    // .............................................................................

    else if (sub === "count") {
      result = collectionRepresentation(collection, true, true, false);
      headers = {
        location : databasePrefix(req, "/" + API + "/" + collection.name() + "/count")
      };
      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/properties
    // .............................................................................

    else if (sub === "properties") {
      result = collectionRepresentation(collection, true, false, false);
      headers = {
        location : databasePrefix(req, "/" + API + "/" + collection.name() + "/properties")
      };
      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/revision
    // .............................................................................

    else if (sub === "revision") {
      result = collectionRepresentation(collection, false, false, false);
      result.revision = collection.revision();
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }

    else {
      actions.resultNotFound(req, res, arangodb.ERROR_HTTP_NOT_FOUND,
                             "expecting one of the resources 'count',"
                             +" 'figures', 'properties', 'parameter'");
    }
  }
  else {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expect GET /" + API + "/<collection-name>/<method>");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_collection_load
/// @brief loads a collection
///
/// @RESTHEADER{PUT /_api/collection/{collection-name}/load, Load collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// Loads a collection into memory. Returns the collection on success.
///
/// The request might optionally contain the following attribute:
///
/// - *count*: If set, this controls whether the return value should include
///   the number of documents in the collection. Setting *count* to
///   *false* may speed up loading a collection. The default value for
///   *count* is *true*.
///
/// On success an object with the following attributes is returned:
///
/// - *id*: The identifier of the collection.
///
/// - *name*: The name of the collection.
///
/// - *count*: The number of documents inside the collection. This is only
///   returned if the *count* input parameters is set to *true* or has
///   not been specified.
///
/// - *status*: The status of the collection as number.
///
/// - *type*: The collection type. Valid types are:
///   - 2: document collection
///   - 3: edges collection
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404*
/// is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierLoad}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn, { waitForSync: true });
///     var url = "/_api/collection/"+ coll.name() + "/load";
///
///     var response = logCurlRequest('PUT', url, '');
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_collection_load (req, res, collection) {
  try {
    collection.load();

    var showCount = true;
    var body = actions.getJsonBody(req, res);

    if (body && body.hasOwnProperty("count")) {
      showCount = body.count;
    }

    var result = collectionRepresentation(collection, false, showCount, false);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_collection_unload
/// @brief unloads a collection
///
/// @RESTHEADER{PUT /_api/collection/{collection-name}/unload, Unload collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
///
/// @RESTDESCRIPTION
/// Removes a collection from memory. This call does not delete any documents.
/// You can use the collection afterwards; in which case it will be loaded into
/// memory, again. On success an object with the following attributes is
/// returned:
///
/// - *id*: The identifier of the collection.
///
/// - *name*: The name of the collection.
///
/// - *status*: The status of the collection as number.
///
/// - *type*: The collection type. Valid types are:
///   - 2: document collection
///   - 3: edges collection
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierUnload}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn, { waitForSync: true });
///     var url = "/_api/collection/"+ coll.name() + "/unload";
///
///     var response = logCurlRequest('PUT', url, '');
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_collection_unload (req, res, collection) {
  try {
    collection.unload();

    var result = collectionRepresentation(collection);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_collection_truncate
/// @brief truncates a collection
///
/// @RESTHEADER{PUT /_api/collection/{collection-name}/truncate, Truncate collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// Removes all documents from the collection, but leaves the indexes intact.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierTruncate}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn, { waitForSync: true });
///     var url = "/_api/collection/"+ coll.name() + "/truncate";
///
///     var response = logCurlRequest('PUT', url, '');
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_collection_truncate (req, res, collection) {
  try {
    collection.truncate();

    var result = collectionRepresentation(collection);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_collection_properties
/// @brief changes a collection
///
/// @RESTHEADER{PUT /_api/collection/{collection-name}/properties, Change properties of a collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// Changes the properties of a collection. Expects an object with the
/// attribute(s)
///
/// - *waitForSync*: If *true* then creating or changing a
///   document will wait until the data has been synchronised to disk.
///
/// - *journalSize*: The maximal size of a journal or datafile in bytes. 
///   The value must be at least `1048576` (1 MB). Note that when
///   changing the journalSize value, it will only have an effect for
///   additional journals or datafiles that are created. Already
///   existing journals or datafiles will not be affected.
///
/// If returns an object with the attributes
///
/// - *id*: The identifier of the collection.
///
/// - *name*: The name of the collection.
///
/// - *waitForSync*: The new value.
///
/// - *journalSize*: The new value.
///
/// - *status*: The status of the collection as number.
///
/// - *type*: The collection type. Valid types are:
///   - 2: document collection
///   - 3: edges collection
///
/// **Note**: some other collection properties, such as *type*, *isVolatile*,
/// *numberOfShards* or *shardKeys* cannot be changed once a collection is
/// created.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierPropertiesSync}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn, { waitForSync: true });
///     var url = "/_api/collection/"+ coll.name() + "/properties";
///
///     var response = logCurlRequest('PUT', url, {"waitForSync" : true });
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_collection_properties (req, res, collection) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  try {
    collection.properties(body);

    var result = collectionRepresentation(collection, true);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_collection_rename
/// @brief renames a collection
///
/// @RESTHEADER{PUT /_api/collection/{collection-name}/rename, Rename collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection to rename.
///
/// @RESTDESCRIPTION
/// Renames a collection. Expects an object with the attribute(s)
///
/// - *name*: The new name.
///
/// If returns an object with the attributes
///
/// - *id*: The identifier of the collection.
///
/// - *name*: The new name of the collection.
///
/// - *status*: The status of the collection as number.
///
/// - *type*: The collection type. Valid types are:
///   - 2: document collection
///   - 3: edges collection
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierRename}
///     var cn = "products1";
///     var coll = db._create(cn);
///     var url = "/_api/collection/"+ coll.name() + "/rename";
///
///     var response = logCurlRequest('PUT', url, { name: "newname" });
///
///     assert(response.code === 200);
///     db._flushCache();
///     db._drop("newname");
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_collection_rename (req, res, collection) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  if (! body.hasOwnProperty("name")) {
    actions.resultBad(req, res, arangodb.ERROR_ARANGO_ILLEGAL_NAME,
                      "name must be non-empty");
    return;
  }

  var name = body.name;

  try {
    collection.rename(name);

    var result = collectionRepresentation(collection);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_collection_rotate
/// @brief rotates the journal of a collection
///
/// @RESTHEADER{PUT /_api/collection/{collection-name}/rotate, Rotate journal of a collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection.
///
/// @RESTDESCRIPTION
/// Rotates the journal of a collection. The current journal of the collection will be closed
/// and made a read-only datafile. The purpose of the rotate method is to make the data in
/// the file available for compaction (compaction is only performed for read-only datafiles, and
/// not for journals).
///
/// Saving new data in the collection subsequently will create a new journal file
/// automatically if there is no current journal.
///
/// If returns an object with the attributes
///
/// - *result*: will be *true* if rotation succeeded
///
/// **Note**: This method is not available in a cluster.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the collection currently has no journal, *HTTP 500* is returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Rotating a journal:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionRotate}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn);
///     coll.save({ "test" : true });
///     require("internal").wal.flush(true, true);
///
///     var url = "/_api/collection/"+ coll.name() + "/rotate";
///     var response = logCurlRequest('PUT', url, { });
///
///     assert(response.code === 200);
///     db._flushCache();
///     db._drop(cn);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Rotating without a journal:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionRotateNoJournal}
///     var cn = "products";
///     db._drop(cn);
///     var coll = db._create(cn);
///     var url = "/_api/collection/"+ coll.name() + "/rotate";
///
///     var response = logCurlRequest('PUT', url, { });
///
///     assert(response.code === 400);
///     db._flushCache();
///     db._drop(cn);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_collection_rotate (req, res, collection) {
  try {
    collection.rotate();

    actions.resultOk(req, res, actions.HTTP_OK, { result: true });
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a collection
////////////////////////////////////////////////////////////////////////////////

function put_api_collection (req, res) {
  if (req.suffix.length !== 2) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expected PUT /" + API + "/<collection-name>/<action>");
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var sub = decodeURIComponent(req.suffix[1]);

  if (sub === "load") {
    put_api_collection_load(req, res, collection);
  }
  else if (sub === "unload") {
    put_api_collection_unload(req, res, collection);
  }
  else if (sub === "truncate") {
    put_api_collection_truncate(req, res, collection);
  }
  else if (sub === "properties") {
    put_api_collection_properties(req, res, collection);
  }
  else if (sub === "rename") {
    put_api_collection_rename(req, res, collection);
  }
  else if (sub === "rotate") {
    put_api_collection_rotate(req, res, collection);
  }
  else {
    actions.resultNotFound(req, res, arangodb.ERROR_HTTP_NOT_FOUND,
                           "expecting one of the actions 'load', 'unload',"
                           + " 'truncate', 'properties', 'rename'");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_delete_api_collection
/// @brief deletes a collection
///
/// @RESTHEADER{DELETE /_api/collection/{collection-name}, Delete collection}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-name,string,required}
/// The name of the collection to delete.
///
/// @RESTDESCRIPTION
/// Deletes a collection identified by *collection-name*.
///
/// If the collection was successfully deleted then, an object is returned with
/// the following attributes:
///
/// - *error*: *false*
///
/// - *id*: The identifier of the deleted collection.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the *collection-name* is missing, then a *HTTP 400* is
/// returned.
///
/// @RESTRETURNCODE{404}
/// If the *collection-name* is unknown, then a *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// Using an identifier:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionDeleteCollectionIdentifier}
///     var cn = "products1";
///     var coll = db._create(cn, { waitForSync: true });
///     var url = "/_api/collection/"+ coll._id;
///
///     var response = logCurlRequest('DELETE', url);
///     db[cn] = undefined;
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a name:
///
/// @EXAMPLE_ARANGOSH_RUN{RestCollectionDeleteCollectionName}
///     var cn = "products1";
///     db._drop(cn);
///     db._create(cn);
///     var url = "/_api/collection/products1";
///
///     var response = logCurlRequest('DELETE', url);
///     db[cn] = undefined;
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function delete_api_collection (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expected DELETE /" + API + "/<collection-name>");
  }
  else {
    var name = decodeURIComponent(req.suffix[0]);
    var collection = arangodb.db._collection(name);

    if (collection === null) {
      actions.collectionNotFound(req, res, name);
    }
    else {
      try {
        var result = {
          id : collection._id
        };

        collection.drop();

        actions.resultOk(req, res, actions.HTTP_OK, result);
      }
      catch (err) {
        actions.resultException(req, res, err, undefined, false);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a collection request
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_collection(req, res);
      }
      else if (req.requestType === actions.DELETE) {
        delete_api_collection(req, res);
      }
      else if (req.requestType === actions.POST) {
        post_api_collection(req, res);
      }
      else if (req.requestType === actions.PUT) {
        put_api_collection(req, res);
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
