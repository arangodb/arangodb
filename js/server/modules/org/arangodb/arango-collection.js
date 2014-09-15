/*jshint strict: false */
/*global ArangoClusterComm, ArangoClusterInfo, require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoCollection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2013 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

var ArangoCollection = internal.ArangoCollection;
exports.ArangoCollection = ArangoCollection;

// must be called after exporting ArangoCollection
require("org/arangodb/arango-collection-common");

var simple = require("org/arangodb/simple-query");
var ArangoError = require("org/arangodb").ArangoError;
var ArangoDatabase = require("org/arangodb/arango-database").ArangoDatabase;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief converts collection into an array
/// @startDocuBlock collectionToArray
/// `collection.toArray()`
///
/// Converts the collection into an array of documents. Never use this call
/// in a production environment.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toArray = function () {
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    return this.all().toArray();
  }

  return this.ALL(null, null).documents;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
/// @startDocuBlock collectionTruncate
/// `collection.truncate()`
///
/// Truncates a *collection*, removing all documents but keeping all its
/// indexes.
///
/// @EXAMPLES
///
/// Truncates a collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionTruncate}
/// ~ db._create("example");
///   col = db.example;
///   col.save({ "Hello" : "World" });
///   col.count();
///   col.truncate();
///   col.count();
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    if (this.status() === ArangoCollection.STATUS_UNLOADED) {
      this.load();
    }
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };

    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/collection/" + encodeURIComponent(shard) + "/truncate",
                                     "",
                                     { },
                                     options);
    });

    cluster.wait(coord, shards);
    return;
  }

  return this.TRUNCATE();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   index functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an index of a collection
///
/// @FUN{@FA{collection}.index(@FA{index-handle})}
///
/// Returns the index with @FA{index-handle} or null if no such index exists.
///
/// *Examples*
///
/// @code
/// arango> db.example.getIndexes().map(function(x) { return x.id; });
/// ["example/0"]
/// arango> db.example.index("93013/0");
/// { "id" : "example/0", "type" : "primary", "fields" : ["_id"] }
/// @endcode
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.index = function (id) {
  var indexes = this.getIndexes();
  var i;

  if (typeof id === "object" && id.hasOwnProperty("id")) {
    id = id.id;
  }

  if (typeof id === "string") {
    var pa = ArangoDatabase.indexRegex.exec(id);

    if (pa === null) {
      id = this.name() + "/" + id;
    }
  }
  else if (typeof id === "number") {
    // stringify the id
    id = this.name() + "/" + id;
  }

  for (i = 0;  i < indexes.length;  ++i) {
    var index = indexes[i];

    if (index.id === id) {
      return index;
    }
  }

  var err = new ArangoError();
  err.errorNum = internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code;
  err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.message;

  throw err;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    edge functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns connected edges
////////////////////////////////////////////////////////////////////////////////

function getEdges (collection, vertex, direction) {
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, collection.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };

    if (vertex !== null &&
        typeof vertex === "object" &&
        vertex.hasOwnProperty("_id")) {
      vertex = vertex._id;
    }

    shards.forEach(function (shard) {
      var url = "/_api/edges/" + encodeURIComponent(shard) +
                "?direction=" + encodeURIComponent(direction) +
                "&vertex=" + encodeURIComponent(vertex);

      ArangoClusterComm.asyncRequest("get",
                                     "shard:" + shard,
                                     dbName,
                                     url,
                                     "",
                                     { },
                                     options);
    });

    var results = cluster.wait(coord, shards), i;
    var edges = [ ];

    for (i = 0; i < results.length; ++i) {
      var body = JSON.parse(results[i].body);

      edges = edges.concat(body.edges);
    }

    return edges;
  }

  if (direction === "in") {
    return collection.INEDGES(vertex);
  }
  if (direction === "out") {
    return collection.OUTEDGES(vertex);
  }

  return collection.EDGES(vertex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all edges connected to a vertex
/// @startDocuBlock collectionEdgesAll
/// `collection.edges(vertex-id)`
///
/// Returns all edges connected to the vertex specified by *vertex-id*.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.edges = function (vertex) {
  return getEdges(this, vertex, "any");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns inbound edges connected to a vertex
/// @startDocuBlock collectionEdgesInbound
/// `collection.edges(vertex-id)`
///
/// Returns inbound edges connected to the vertex specified by *vertex-id*.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.inEdges = function (vertex) {
  return getEdges(this, vertex, "in");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns outbound edges connected to a vertex
/// @startDocuBlock collectionEdgesOutbound
/// `collection.edges(vertex-id)`
///
/// Returns outbound edges connected to the vertex specified by *vertex-id*.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.outEdges = function (vertex) {
  return getEdges(this, vertex, "out");
};

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns any document from a collection
/// @startDocuBlock documentsCollectionAny
/// `collection.any()`
///
/// Returns a random document from the collection or *null* if none exists.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.any = function () {
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };

    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/any",
                                     JSON.stringify({
                                       collection: shard
                                     }),
                                     { },
                                     options);
    });

    var results = cluster.wait(coord, shards), i;
    for (i = 0; i < results.length; ++i) {
      var body = JSON.parse(results[i].body);
      if (body.document !== null) {
        return body.document;
      }
    }

    return null;
  }

  return this.ANY();
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_ArangoCollection_prototype_first
/// @brief selects the n first documents in the collection
/// @startDocuBlock documentsCollectionFirst
/// `collection.first(count)`
///
/// The *first* method returns the n first documents from the collection, in
/// order of document insertion/update time.
///
/// If called with the *count* argument, the result is a list of up to
/// *count* documents. If *count* is bigger than the number of documents
/// in the collection, then the result will contain as many documents as there
/// are in the collection.
/// The result list is ordered, with the "oldest" documents being positioned at
/// the beginning of the result list.
///
/// When called without an argument, the result is the first document from the
/// collection. If the collection does not contain any documents, the result
/// returned is *null*.
///
/// **Note**: this method is not supported in sharded collections with more than
/// one shard.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionFirst}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
/// ~ db.example.save({ Foo : "bar" });
///   db.example.first(1);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionFirstNull}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
///   db.example.first();
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.first = function (count) {
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this.name());

    if (shards.length !== 1) {
      var err = new ArangoError();
      err.errorNum = internal.errors.ERROR_CLUSTER_UNSUPPORTED.code;
      err.errorMessage = "operation is not supported in sharded collections";

      throw err;
    }

    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
    var shard = shards[0];

    ArangoClusterComm.asyncRequest("put",
                                   "shard:" + shard,
                                   dbName,
                                   "/_api/simple/first",
                                   JSON.stringify({
                                     collection: shard,
                                     count: count
                                   }),
                                   { },
                                   options);

    var results = cluster.wait(coord, shards);

    if (results.length) {
      var body = JSON.parse(results[0].body);
      return body.result || null;
    }
  }
  else {
    return this.FIRST(count);
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_ArangoCollection_prototype_last
///
/// @brief selects the n last documents in the collection
/// @startDocuBlock documentsCollectionLast
/// `collection.last(count)`
///
/// The *last* method returns the n last documents from the collection, in
/// order of document insertion/update time.
///
/// If called with the *count* argument, the result is a list of up to
/// *count* documents. If *count* is bigger than the number of documents
/// in the collection, then the result will contain as many documents as there
/// are in the collection.
/// The result list is ordered, with the "latest" documents being positioned at
/// the beginning of the result list.
///
/// When called without an argument, the result is the last document from the
/// collection. If the collection does not contain any documents, the result
/// returned is *null*.
///
/// **Note**: this method is not supported in sharded collections with more than
/// one shard.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionLast}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
/// ~ db.example.save({ Foo : "bar" });
///   db.example.last(2);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionLastNull}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
///   db.example.last(1);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.last = function (count) {
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this.name());

    if (shards.length !== 1) {
      var err = new ArangoError();
      err.errorNum = internal.errors.ERROR_CLUSTER_UNSUPPORTED.code;
      err.errorMessage = "operation is not supported in sharded collections";

      throw err;
    }

    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
    var shard = shards[0];

    ArangoClusterComm.asyncRequest("put",
                                   "shard:" + shard,
                                   dbName,
                                   "/_api/simple/last",
                                   JSON.stringify({
                                     collection: shard,
                                     count: count
                                   }),
                                   { },
                                   options);

    var results = cluster.wait(coord, shards);

    if (results.length) {
      var body = JSON.parse(results[0].body);
      return body.result || null;
    }
  }
  else {
    return this.LAST(count);
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_ArangoCollection_prototype_firstExample
/// @brief constructs a query-by-example for a collection
/// @startDocuBlock collectionFirstExample
/// `collection.firstExample(example)`
///
/// Returns the first document of a collection that matches the specified
/// example or *null*. The example must be specified as paths and values.
/// See *byExample* for details.
///
/// `collection.firstExample(path1, value1, ...)`
///
/// As alternative you can supply a list of paths and values.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionFirstExample}
/// ~ db._create("users");
/// ~ db.users.save({ name: "Gerhard" });
/// ~ db.users.save({ name: "Helmut" });
/// ~ db.users.save({ name: "Angela" });
///   db.users.firstExample("name", "Angela");
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.firstExample = function (example) {
  var e;
  var i;

  // example is given as only argument
  if (arguments.length === 1) {
    e = example;
  }

  // example is given as list
  else {
    e = {};

    for (i = 0;  i < arguments.length;  i += 2) {
      e[arguments[i]] = arguments[i + 1];
    }
  }

  var documents = (new simple.SimpleQueryByExample(this, e)).limit(1).toArray();
  if (documents.length > 0) {
    return documents[0];
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents matching an example
/// @param example a json object which is used as sample for finding docs that
///        this example matches
/// @param waitForSync (optional) a boolean value or a json object
/// @param limit (optional) an integer value, the max number of to deleting docs
/// @example remove("{a : 1 }")
/// @example remove("{a : 1 }", true)
/// @example remove("{a : 1 }", false)
/// @example remove("{a : 1 }", {waitForSync: false, limit: 12})
/// @throws "too many parameters" when waiitForSyn is a Json object and limit
///         is given
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example,
                                                       waitForSync,
                                                       limit) {
  if (limit === 0) {
    return 0;
  }
  if (typeof waitForSync === "object") {
    if (typeof limit !== "undefined") {
      throw "too many parameters";
    }
    var tmp_options = waitForSync === null ? {} : waitForSync;
    // avoiding jslint error
    // see: http://jslinterrors.com/unexpected-sync-method-a/
        waitForSync = tmp_options.waitForSync;
    limit = tmp_options.limit;
  }
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };

    if (limit > 0 && shards.length > 1) {
      var err = new ArangoError();
      err.errorNum = internal.errors.ERROR_CLUSTER_UNSUPPORTED.code;
      err.errorMessage = "limit is not supported in sharded collections";

      throw err;
    }

    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/remove-by-example",
                                     JSON.stringify({
                                       collection: shard,
                                       example: example,
                                       waitForSync: waitForSync,
                                       limit: limit || undefined
                                     }),
                                     { },
                                     options);
    });

    var deleted = 0;
    var results = cluster.wait(coord, shards), i;
    for (i = 0; i < results.length; ++i) {
      var body = JSON.parse(results[i].body);

      deleted += (body.deleted || 0);
    }

    return deleted;
  }

  return require("internal").db._executeTransaction({
    collections: {
      write: this.name()
    },
    action: function (params) {
      var collection = params.c;
      var documents = collection.byExample(params.example);
      if (params.limit > 0) {
        documents = documents.limit(params.limit);
      }

      var deleted = 0;
      while (documents.hasNext()) {
        var document = documents.next();

        if (collection.remove(document, true, params.wfs)) {
          deleted++;
        }
      }
      return deleted;
    },
    params: {
      c: this,
      example: example,
      limit: limit,
      wfs: waitForSync
    }
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces documents matching an example
/// @param example the json object for finding documents which matches this
///        example
/// @param newValue the json object which replaces the matched documents
/// @param waitForSync (optional) a boolean value or a json object which
///        contains the options
/// @param limit (optional) an integer value, the max number of to deleting docs
/// @throws "too many parameters" when waitForSync is a json object and limit
///        was given
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example,
                                                        newValue,
                                                        waitForSync,
                                                        limit) {
  if (limit === 0) {
    return 0;
  }

  if (typeof newValue !== "object" || Array.isArray(newValue)) {
    var err1 = new ArangoError();
    err1.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err1.errorMessage = "invalid value for parameter 'newValue'";

    throw err1;
  }

  if (typeof waitForSync === "object") {
    if (typeof limit !== "undefined") {
      throw "too many parameters";
    }
    var tmp_options = waitForSync === null ? {} : waitForSync;
    // avoiding jslint error
    // see: http://jslinterrors.com/unexpected-sync-method-a/
        waitForSync = tmp_options.waitForSync;
    limit = tmp_options.limit;
  }

  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };

    if (limit > 0 && shards.length > 1) {
      var err2 = new ArangoError();
      err2.errorNum = internal.errors.ERROR_CLUSTER_UNSUPPORTED.code;
      err2.errorMessage = "limit is not supported in sharded collections";

      throw err2;
    }

    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/replace-by-example",
                                     JSON.stringify({
                                       collection: shard,
                                       example: example,
                                       newValue: newValue,
                                       waitForSync: waitForSync,
                                       limit: limit || undefined
                                     }),
                                     { },
                                     options);
    });

    var replaced = 0;
    var results = cluster.wait(coord, shards), i;
    for (i = 0; i < results.length; ++i) {
      var body = JSON.parse(results[i].body);

      replaced += (body.replaced || 0);
    }

    return replaced;
  }

  return require("internal").db._executeTransaction({
    collections: {
      write: this.name()
    },
    action: function (params) {
      var collection = params.c;
      var documents = collection.byExample(params.example);
      if (params.limit > 0) {
        documents = documents.limit(params.limit);
      }

      var replaced = 0;
      while (documents.hasNext()) {
        var document = documents.next();

        if (collection.replace(document, params.newValue, true, params.wfs)) {
          replaced++;
        }
      }
      return replaced;
    },
    params: {
      c: this,
      example: example,
      newValue: newValue,
      limit: limit,
      wfs: waitForSync
    }
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief partially updates documents matching an example
/// @param example the json object for finding documents which matches this
///        example
/// @param newValue the json object which replaces the matched documents
/// @param keepNull (optional) true or a json object which
///        contains the options
/// @param waitForSync (optional) a boolean value
/// @param limit (optional) an integer value, the max number of to deleting docs
/// @throws "too many parameters" when keepNull is  a json object and limit
///        was given
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example,
                                                       newValue,
                                                       keepNull,
                                                       waitForSync,
                                                       limit) {

  if (limit === 0) {
    return 0;
  }

  if (typeof newValue !== "object" || Array.isArray(newValue)) {
    var err1 = new ArangoError();
    err1.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err1.errorMessage = "invalid value for parameter 'newValue'";

    throw err1;
  }

  if (typeof keepNull === "object") {
    if (typeof waitForSync !== "undefined") {
      throw "too many parameters";
    }
    var tmp_options = keepNull === null ? {} : keepNull;

    // avoiding jslint error
    // see: http://jslinterrors.com/unexpected-sync-method-a/
        keepNull = tmp_options.keepNull;
    waitForSync = tmp_options.waitForSync;
    limit = tmp_options.limit;
  }

  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };

    if (limit > 0 && shards.length > 1) {
      var err2 = new ArangoError();
      err2.errorNum = internal.errors.ERROR_CLUSTER_UNSUPPORTED.code;
      err2.errorMessage = "limit is not supported in sharded collections";

      throw err2;
    }

    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/update-by-example",
                                     JSON.stringify({
                                       collection: shard,
                                       example: example,
                                       newValue: newValue,
                                       waitForSync: waitForSync,
                                       keepNull: keepNull,
                                       limit: limit || undefined
                                     }),
                                     { },
                                     options);
    });

    var updated = 0;
    var results = cluster.wait(coord, shards), i;
    for (i = 0; i < results.length; ++i) {
      var body = JSON.parse(results[i].body);

      updated += (body.updated || 0);
    }

    return updated;
  }

  return require("internal").db._executeTransaction({
    collections: {
      write: this.name()
    },
    action: function (params) {
      var collection = params.c;
      var documents = collection.byExample(params.example);
      if (params.limit > 0) {
        documents = documents.limit(params.limit);
      }

      var updated = 0;
      while (documents.hasNext()) {
        var document = documents.next();

        if (collection.update(document, params.newValue,
            {overwrite: true, keepNull: params.keepNull, waitForSync: params.wfs})) {
          updated++;
        }
      }
      return updated;
    },
    params: {
      c: this,
      example: example,
      newValue: newValue,
      keepNull: keepNull,
      limit: limit,
      wfs: waitForSync
    }
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
/// @startDocuBlock collectionEnsureCapConstraint
/// `collection.ensureCapConstraint(size, byteSize)`
///
/// Creates a size restriction aka cap for the collection of *size*
/// documents and/or *byteSize* data size. If the restriction is in place
/// and the (*size* plus one) document is added to the collection, or the
/// total active data size in the collection exceeds *byteSize*, then the
/// least recently created or updated documents are removed until all
/// constraints are satisfied.
///
/// It is allowed to specify either *size* or *byteSize*, or both at
/// the same time. If both are specified, then the automatic document removal
/// will be triggered by the first non-met constraint.
///
/// Note that at most one cap constraint is allowed per collection. Trying
/// to create additional cap constraints will result in an error. Creating
/// cap constraints is also not supported in sharded collections with more
/// than one shard.
///
/// Note that this does not imply any restriction of the number of revisions
/// of documents.
///
/// @EXAMPLES
///
/// Restrict the number of document to at most 10 documents:
///
/// @verbinclude ensure-cap-constraint
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureCapConstraint = function (size, byteSize) {
  "use strict";

  return this.ensureIndex({
    type: "cap",
    size: size || undefined,
    byteSize: byteSize || undefined
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
/// @startDocuBlock ensureUniqueSkiplist
/// `ensureUniqueSkiplist(field*1*, field*2*, ...,field*n*)`
///
/// Creates a skiplist index on all documents using attributes as paths to
/// the fields. At least one attribute must be given.
/// All documents, which do not have the attribute path or
/// with one or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude unique-skiplist
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueSkiplist = function () {
  "use strict";

  return this.ensureIndex({
    type: "skiplist",
    fields: Array.prototype.slice.call(arguments),
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a multi skiplist index exists
/// @startDocuBlock ensureSkiplist
/// `ensureSkiplist(field*1*, field*2*, ...,field*n*)`
///
/// Creates a multi skiplist index on all documents using attributes as paths to
/// the fields. At least one attribute must be given.
/// All documents, which do not have the attribute path or
/// with one or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude multi-skiplist
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureSkiplist = function () {
  "use strict";

  return this.ensureIndex({
    type: "skiplist",
    fields: Array.prototype.slice.call(arguments)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
/// @startDocuBlock ensureFulltextIndex
/// `ensureFulltextIndex(field, minWordLength)`
///
/// Creates a fulltext index on all documents on attribute *field*.
/// All documents, which do not have the attribute *field* or that have a
/// non-textual value inside their *field* attribute are ignored.
///
/// The minimum length of words that are indexed can be specified with the
/// @FA{minWordLength} parameter. Words shorter than *minWordLength*
/// characters will not be indexed. *minWordLength* has a default value of 2,
/// but this value might be changed in future versions of ArangoDB. It is thus
/// recommended to explicitly specify this value
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude fulltext
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureFulltextIndex = function (field, minLength) {
  "use strict";

  if (! Array.isArray(field)) {
    field = [ field ];
  }

  return this.ensureIndex({
    type: "fulltext",
    minLength: minLength || undefined,
    fields: field
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a unique constraint exists
/// @startDocuBlock ensureUniqueConstraint
/// `ensureUniqueConstraint(field*1*, field*2*, ...,field*n*)`
///
/// Creates a unique hash index on all documents using *field1*, *field2*,
/// ... as attribute paths. At least one attribute path must be given.
///
/// When a unique constraint is in effect for a collection, then all documents
/// which contain the given attributes must differ in the attribute
/// values. Creating a new document or updating a document will fail, if the
/// uniqueness is violated. If any attribute value is null for a document, this
/// document is ignored by the index.
///
/// Note that non-existing attribute paths in a document are treated as if the
/// value were *null*.
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// *Examples*
///
/// @verbinclude shell-index-create-unique-constraint
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueConstraint = function () {
  "use strict";

  return this.ensureIndex({
    type: "hash",
    fields: Array.prototype.slice.call(arguments),
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
/// @startDocuBlock ensureHashIndex
/// `ensureHashIndex(field*1*, field*2*, ...,field*n*)`
///
/// Creates a non-unique hash index on all documents using *field1*, *field2*,
/// ... as attribute paths. At least one attribute path must be given.
///
/// Note that non-existing attribute paths in a document are treated as if the
/// value were *null*.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// *Examples*
///
/// @verbinclude shell-index-create-hash-index
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureHashIndex = function () {
  "use strict";

  return this.ensureIndex({
    type: "hash",
    fields: Array.prototype.slice.call(arguments)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
/// @startDocuBlock collectionEnsureGeoIndex
/// `collection.ensureGeoIndex(location)`
///
/// Creates a geo-spatial index on all documents using *location* as path to
/// the coordinates. The value of the attribute must be a list with at least two
/// double values. The list must contain the latitude (first value) and the
/// longitude (second value). All documents, which do not have the attribute
/// path or with value that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// `collection.ensureGeoIndex(location, true)`
///
/// As above which the exception, that the order within the list is longitude
/// followed by latitude. This corresponds to the format described in
/// [positions](http://geojson.org/geojson-spec.html)
///
/// `collection.ensureGeoIndex(latitude, longitude)`
///
/// Creates a geo-spatial index on all documents using *latitude* and
/// *longitude* as paths the latitude and the longitude. The value of the
/// attribute *latitude* and of the attribute *longitude* must a
/// double. All documents, which do not have the attribute paths or which values
/// are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @EXAMPLES
///
/// Create an geo index for a list attribute:
///
/// @verbinclude ensure-geo-index-list
///
/// Create an geo index for a hash array attribute:
///
/// @verbinclude ensure-geo-index-array
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoIndex = function (lat, lon) {
  "use strict";

  if (typeof lat !== "string") {
    throw "usage: ensureGeoIndex(<lat>, <lon>) or ensureGeoIndex(<loc>[, <geoJson>])";
  }

  if (typeof lon === "boolean") {
    return this.ensureIndex({
      type : "geo1",
      fields : [ lat ],
      geoJson : lon
    });
  }

  if (lon === undefined) {
    return this.ensureIndex({
      type : "geo1",
      fields : [ lat ],
      geoJson : false
    });
  }

  return this.ensureIndex({
    type : "geo2",
    fields : [ lat, lon ]
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo constraint exists
/// @startDocuBlock collectionEnsureGeoConstraint
/// `collection.ensureGeoConstraint(location, ignore-null)`
///
/// `collection.ensureGeoConstraint(location, true, ignore-null)`
///
/// `collection.ensureGeoConstraint(latitude, longitude, ignore-null)`
///
/// Works like *ensureGeoIndex* but requires that the documents contain
/// a valid geo definition. If *ignore-null* is true, then documents with
/// a null in *location* or at least one null in *latitude* or
/// *longitude* are ignored.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoConstraint = function (lat, lon, ignoreNull) {
  "use strict";

  // only two parameter
  if (ignoreNull === undefined) {
    ignoreNull = lon;

    if (typeof ignoreNull !== "boolean") {
      throw "usage: ensureGeoConstraint(<lat>, <lon>, <ignore-null>)"
          + " or ensureGeoConstraint(<lat>, <geo-json>, <ignore-null>)";
    }

    return this.ensureIndex({
      type : "geo1",
      fields : [ lat ],
      geoJson : false,
      unique : true,
      ignoreNull : ignoreNull
    });
  }

  // three parameter
  if (typeof lon === "boolean") {
    return this.ensureIndex({
      type : "geo1",
      fields : [ lat ],
      geoJson : lon,
      unique : true,
      ignoreNull : ignoreNull
    });
  }

  return this.ensureIndex({
    type : "geo2",
    fields : [ lat, lon ],
    unique : true,
    ignoreNull : ignoreNull
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a unique constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupUniqueConstraint = function () {
  "use strict";

  return this.lookupIndex({
    type: "hash",
    fields: Array.prototype.slice.call(arguments),
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a hash index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupHashIndex = function () {
  "use strict";

  return this.lookupIndex({
    type: "hash",
    fields: Array.prototype.slice.call(arguments)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a unique skiplist index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupUniqueSkiplist = function () {
  "use strict";

  return this.lookupIndex({
    type: "skiplist",
    fields: Array.prototype.slice.call(arguments),
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a skiplist index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupSkiplist = function () {
  "use strict";

  return this.lookupIndex({
    type: "skiplist",
    fields: Array.prototype.slice.call(arguments)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a fulltext index
/// @startDocuBlock lookUpFulltextIndex
/// `lookupFulltextIndex(field, minLength)`
///
/// Checks whether a fulltext index on the given attribute *field* exists.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupFulltextIndex = function (field, minLength) {
  "use strict";

  if (! Array.isArray(field)) {
    field = [ field ];
  }

  return this.lookupIndex({
    type: "fulltext",
    fields: field,
    minLength: minLength || undefined
  });
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
