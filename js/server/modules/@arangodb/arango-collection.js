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

////////////////////////////////////////////////////////////////////////////////
/// @brief builds an example query
////////////////////////////////////////////////////////////////////////////////

function buildExampleQuery (collection, example, limit) {
  var parts = [ ]; 
  var bindVars = { "@collection" : collection.name() };
  var keys = Object.keys(example);

  for (var i = 0; i < keys.length; ++i) {
    var key = keys[i];
    parts.push("doc.@att" + i + " == @value" + i);
    bindVars["att" + i] = key;
    bindVars["value" + i] = example[key];
  }
  
  var query = "FOR doc IN @@collection";
  if (parts.length > 0) {
    query += " FILTER " + parts.join(" && ", parts);
  }
  if (limit > 0) {
    query += " LIMIT " + parseInt(limit, 10);
  }

  return { query: query, bindVars: bindVars };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add options from arguments to index specification
////////////////////////////////////////////////////////////////////////////////

function addIndexOptions (body, parameters) {
  body.fields = [ ];

  var setOption = function(k) {
    if (! body.hasOwnProperty(k)) {
      body[k] = parameters[i][k];
    }
  };

  var i;
  for (i = 0; i < parameters.length; ++i) {
    if (typeof parameters[i] === "string") {
      // set fields
      body.fields.push(parameters[i]);
    }
    else if (typeof parameters[i] === "object" && 
             ! Array.isArray(parameters[i]) &&
             parameters[i] !== null) {
      // set arbitrary options
      Object.keys(parameters[i]).forEach(setOption);
      break;
    }
  }

  return body;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

var ArangoCollection = internal.ArangoCollection;
exports.ArangoCollection = ArangoCollection;

// must be called after exporting ArangoCollection
require("@arangodb/arango-collection-common");

var simple = require("@arangodb/simple-query");
var ArangoError = require("@arangodb").ArangoError;
var ArangoDatabase = require("@arangodb/arango-database").ArangoDatabase;


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionToArray
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toArray = function () {
  var cluster = require("@arangodb/cluster");

  if (cluster.isCoordinator()) {
    return this.all().toArray();
  }

  return this.ALL(null, null).documents;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionTruncate
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
  var cluster = require("@arangodb/cluster");

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


////////////////////////////////////////////////////////////////////////////////
/// @brief returns connected edges
////////////////////////////////////////////////////////////////////////////////

function getEdges (collection, vertex, direction) {
  var cluster = require("@arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, collection.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
    var body;

    if (vertex !== null &&
        typeof vertex === "object" &&
        vertex.hasOwnProperty("_id")) {
      vertex = vertex._id;
    }
    if (Array.isArray(vertex)) {
      var idList = vertex.map(function (v) {
        if (v !== null) {
          if (typeof v === "object" &&
              v.hasOwnProperty("_id")) {
            return v._id;
          }
          if (typeof v === "string") {
            return v;
          }
        }
      });
      body = JSON.stringify(idList);
      shards.forEach(function (shard) {
        var url = "/_api/edges/" + encodeURIComponent(shard) +
                  "?direction=" + encodeURIComponent(direction);

        ArangoClusterComm.asyncRequest("post",
                                       "shard:" + shard,
                                       dbName,
                                       url,
                                       body,
                                       { },
                                       options);
      });

    }
    else {
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
    }

    var results = cluster.wait(coord, shards), i;
    var edges = [ ];

    for (i = 0; i < results.length; ++i) {
      body = JSON.parse(results[i].body);

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
/// @brief was docuBlock collectionEdgesAll
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.edges = function (vertex) {
  return getEdges(this, vertex, "any");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionEdgesInbound
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.inEdges = function (vertex) {
  return getEdges(this, vertex, "in");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionEdgesOutbound
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.outEdges = function (vertex) {
  return getEdges(this, vertex, "out");
};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionAny
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.any = function () {
  var cluster = require("@arangodb/cluster");

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
/// @brief was docuBlock documentsCollectionFirst
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.first = function (count) {
  var cluster = require("@arangodb/cluster");

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
/// @brief was docuBlock documentsCollectionLast
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.last = function (count) {
  var cluster = require("@arangodb/cluster");

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
/// @brief saves a document in the collection
/// note: this method is used to save documents and edges, but save() has a
/// different signature for both. For document collections, the signature is
/// save(<data>, <waitForSync>), whereas for edge collections, the signature is
/// save(<from>, <to>, <data>, <waitForSync>)
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.save =
ArangoCollection.prototype.insert = function (from, to, data, waitForSync) {
  var type = this.type();

  if (type === undefined) {
    type = ArangoCollection.TYPE_DOCUMENT;
  }

  if (type === ArangoCollection.TYPE_DOCUMENT) {
    data = from;
    waitForSync = to;
    return this._insert(data, waitForSync);
  }
  else if (type === ArangoCollection.TYPE_EDGE) {
    if (!data && from && typeof from === 'object' && from.hasOwnProperty("_from") && from.hasOwnProperty("_to")) {
      data = from;
      waitForSync = to;
      from = data._from;
      to = data._to;
    }

    if (typeof from === 'object' && from.hasOwnProperty("_id")) {
      from = from._id;
    }

    if (typeof to === 'object' && to.hasOwnProperty("_id")) {
      to = to._id;
    }
  }

  return this._insert(from, to, data, waitForSync);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionFirstExample
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

  var i;
  var cluster = require("@arangodb/cluster");

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
    var results = cluster.wait(coord, shards);
    for (i = 0; i < results.length; ++i) {
      var body = JSON.parse(results[i].body);
      deleted += (body.deleted || 0);
    }

    return deleted;
  }

  var query = buildExampleQuery(this, example, limit);
  var opts = { waitForSync : waitForSync };
  query.query += " REMOVE doc IN @@collection OPTIONS " + JSON.stringify(opts);

  return require("internal").db._query(query).getExtra().stats.writesExecuted; 
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

  var cluster = require("@arangodb/cluster");

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

  var query = buildExampleQuery(this, example, limit);
  var opts = { waitForSync : waitForSync };
  query.query += " REPLACE doc WITH @newValue IN @@collection OPTIONS " + JSON.stringify(opts);
  query.bindVars.newValue = newValue;

  return require("internal").db._query(query).getExtra().stats.writesExecuted; 
};

////////////////////////////////////////////////////////////////////////////////
/// @brief partially updates documents matching an example
/// @param example the json object for finding documents which matches this
///        example
/// @param newValue the json object which replaces the matched documents
/// @param keepNull (optional) true or a JSON object which
///        contains the options
/// @param waitForSync (optional) a boolean value
/// @param limit (optional) an integer value, the max number of to deleting docs
/// @throws "too many parameters" when keepNull is a JSON object and limit
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

  var mergeObjects = true;

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
    if (tmp_options.hasOwnProperty("mergeObjects")) {
      mergeObjects = tmp_options.mergeObjects || false;
    }
  }

  var cluster = require("@arangodb/cluster");

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
                                       mergeObjects: mergeObjects,
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

  var query = buildExampleQuery(this, example, limit);
  var opts = { waitForSync : waitForSync, keepNull: keepNull, mergeObjects: mergeObjects };
  query.query += " UPDATE doc WITH @newValue IN @@collection OPTIONS " + JSON.stringify(opts);
  query.bindVars.newValue = newValue;

  return require("internal").db._query(query).getExtra().stats.writesExecuted; 
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionEnsureCapConstraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureCapConstraint = function (size, byteSize) {
  'use strict';

  return this.ensureIndex({
    type: "cap",
    size: size || undefined,
    byteSize: byteSize || undefined
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock ensureUniqueSkiplist
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueSkiplist = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: "skiplist",
    unique: true
  }, arguments));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock ensureSkiplist
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureSkiplist = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: "skiplist"
  }, arguments));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock ensureFulltextIndex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureFulltextIndex = function (field, minLength) {
  'use strict';

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
/// @brief was docuBlock ensureUniqueConstraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueConstraint = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: "hash",
    unique: true
  }, arguments));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock ensureHashIndex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureHashIndex = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: "hash"
  }, arguments));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionEnsureGeoIndex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoIndex = function (lat, lon) {
  'use strict';

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
/// @brief was docuBlock collectionEnsureGeoConstraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoConstraint = function (lat, lon) {
  'use strict';

  return this.ensureGeoIndex(lat, lon);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a unique constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupUniqueConstraint = function () {
  'use strict';

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
  'use strict';

  return this.lookupIndex({
    type: "hash",
    fields: Array.prototype.slice.call(arguments)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a unique skiplist index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupUniqueSkiplist = function () {
  'use strict';

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
  'use strict';

  return this.lookupIndex({
    type: "skiplist",
    fields: Array.prototype.slice.call(arguments)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock lookUpFulltextIndex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupFulltextIndex = function (field, minLength) {
  'use strict';

  if (! Array.isArray(field)) {
    field = [ field ];
  }

  return this.lookupIndex({
    type: "fulltext",
    fields: field,
    minLength: minLength || undefined
  });
};


