/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true */
/*global ArangoClusterComm, ArangoClusterInfo, require, exports */

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
///
/// @FUN{@FA{collection}.toArray()}
///
/// Converts the collection into an array of documents. Never use this call
/// in a production environment.
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
///
/// @FUN{@FA{collection}.truncate()}
///
/// Truncates a @FA{collection}, removing all documents but keeping all its
/// indexes.
///
/// @EXAMPLES
///
/// Truncates a collection:
///
/// @code
/// arango> col = db.examples;
/// [ArangoCollection 91022, "examples" (status new born)]
/// arango> col.save({ "Hello" : "World" });
/// { "_id" : "91022/1532814", "_rev" : 1532814 }
/// arango> col.count();
/// 1
/// arango> col.truncate();
/// arango> col.count();
/// 0
/// @endcode
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
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
/// @EXAMPLES
///
/// @code
/// arango> db.example.getIndexes().map(function(x) { return x.id; });
/// ["93013/0"]
/// arango> db.example.index("93013/0");
/// { "id" : "93013/0", "type" : "primary", "fields" : ["_id"] }
/// @endcode
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.index = function (id) {
  var indexes = this.getIndexes();
  var i;

  if (typeof id === "string") {
    var pa = ArangoDatabase.indexRegex.exec(id);

    if (pa === null) {
      id = this.name() + "/" + id;
    }
  }
  else if (id && id.hasOwnProperty("id")) {
    id = id.id;
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

  // index not found
  var err = new ArangoError();
  err.errorNum = internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code;
  err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.message;

  throw err;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns any document from a collection
///
/// @FUN{@FA{collection}.any()}
///
/// Returns a random document from the collection or @LIT{null} if none exists.
///
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
///
/// @brief selects the n first documents in the collection
///
/// @FUN{@FA{collection}.first(@FA{count})}
///
/// The @FN{first} method returns the n first documents from the collection, in 
/// order of document insertion/update time. 
///
/// If called with the @FA{count} argument, the result is a list of up to
/// @FA{count} documents. If @FA{count} is bigger than the number of documents
/// in the collection, then the result will contain as many documents as there
/// are in the collection.
/// The result list is ordered, with the "oldest" documents being positioned at 
/// the beginning of the result list.
///
/// When called without an argument, the result is the first document from the
/// collection. If the collection does not contain any documents, the result 
/// returned is @LIT{null}.
///
/// Note: this method is not supported in sharded collections with more than
/// one shard.
///
/// @EXAMPLES
///
/// @code
/// arangod> db.example.first(1)
/// [ { "_id" : "example/222716379559", "_rev" : "222716379559", "Hello" : "World" } ]
/// @endcode
///
/// @code
/// arangod> db.example.first()
/// { "_id" : "example/222716379559", "_rev" : "222716379559", "Hello" : "World" }
/// @endcode
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
///
/// @FUN{@FA{collection}.last(@FA{count})}
///
/// The @FN{last} method returns the n last documents from the collection, in 
/// order of document insertion/update time. 
///
/// If called with the @FA{count} argument, the result is a list of up to
/// @FA{count} documents. If @FA{count} is bigger than the number of documents
/// in the collection, then the result will contain as many documents as there
/// are in the collection.
/// The result list is ordered, with the "latest" documents being positioned at 
/// the beginning of the result list.
///
/// When called without an argument, the result is the last document from the
/// collection. If the collection does not contain any documents, the result 
/// returned is @LIT{null}.
///
/// Note: this method is not supported in sharded collections with more than
/// one shard.
///
/// @EXAMPLES
///
/// @code
/// arangod> db.example.last(1)
/// [ { "_id" : "example/222716379559", "_rev" : "222716379559", "Hello" : "World" } ]
/// @endcode
///
/// @code
/// arangod> db.example.last()
/// { "_id" : "example/222716379559", "_rev" : "222716379559", "Hello" : "World" }
/// @endcode
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
///
/// @brief constructs a query-by-example for a collection
///
/// @FUN{@FA{collection}.firstExample(@FA{example})}
///
/// Returns the first document of a collection that matches the specified 
/// example or @LIT{null}. The example must be specified as paths and values. 
/// See @FN{byExample} for details.
///
/// @FUN{@FA{collection}.firstExample(@FA{path1}, @FA{value1}, ...)}
///
/// As alternative you can supply a list of paths and values.
///
/// @EXAMPLES
///
/// @TINYEXAMPLE{shell-simple-query-first-example,finds a document with a given name}
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
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example, 
                                                       waitForSync, 
                                                       limit) {
  if (limit === 0) {
    return 0;
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

        if (collection.update(document, params.newValue, true, params.keepNull, params.wfs)) {
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
/// @brief ensures that a bitarray index exists
///
/// @FUN{@FA{collection}.ensureBitarray(@FA{field1}, @FA{value1}, ..., @FA{fieldn}, @FA{valuen})}
///
/// Creates a bitarray index on documents using attributes as paths to the
/// fields (@FA{field1},..., @FA{fieldn}). A value (@FA{value1},...,@FA{valuen})
/// consists of an array of possible values that the field can take. At least
/// one field and one set of possible values must be given.
///
/// All documents, which do not have *all* of the attribute paths are ignored
/// (that is, are not part of the bitarray index, they are however stored within
/// the collection). A document which contains all of the attribute paths yet
/// has one or more values which are *not* part of the defined range of values
/// will be rejected and the document will not inserted within the
/// collection. Note that, if a bitarray index is created subsequent to
/// any documents inserted in the given collection, then the creation of the
/// index will fail if one or more documents are rejected (due to
/// attribute values being outside the designated range).
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// In the example below we create a bitarray index with one field and that
/// field can have the values of either `0` or `1`. Any document which has the
/// attribute `x` defined and does not have a value of `0` or `1` will be
/// rejected and therefore not inserted within the collection. Documents without
/// the attribute `x` defined will not take part in the index.
///
/// @code
/// arango> arangod> db.example.ensureBitarray("x", [0,1]);
/// {
///   "id" : "2755894/3607862",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1]]],
///   "undefined" : false,
///   "isNewlyCreated" : true
/// }
/// @endcode
///
/// In the example below we create a bitarray index with one field and that
/// field can have the values of either `0`, `1` or *other* (indicated by
/// `[]`). Any document which has the attribute `x` defined will take part in
/// the index. Documents without the attribute `x` defined will not take part in
/// the index.
///
/// @code
/// arangod> db.example.ensureBitarray("x", [0,1,[]]);
/// {
///   "id" : "2755894/4263222",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1, [ ]]]],
///   "undefined" : false,
///   "isNewlyCreated" : true
/// }
/// @endcode
///
/// In the example below we create a bitarray index with two fields. Field `x`
/// can have the values of either `0` or `1`; while field `y` can have the values
/// of `2` or `"a"`. A document which does not have *both* attributes `x` and `y`
/// will not take part within the index.  A document which does have both attributes
/// `x` and `y` defined must have the values `0` or `1` for attribute `x` and
/// `2` or `a` for attribute `y`, otherwise the document will not be inserted
/// within the collection.
///
/// @code
/// arangod> db.example.ensureBitarray("x", [0,1], "y", [2,"a"]);
/// {
///   "id" : "2755894/5246262",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1]], ["y", [0, 1]]],
///   "undefined" : false,
///   "isNewlyCreated" : false
/// }
/// @endcode
///
/// In the example below we create a bitarray index with two fields. Field `x`
/// can have the values of either `0` or `1`; while field `y` can have the
/// values of `2`, `"a"` or *other* . A document which does not have *both*
/// attributes `x` and `y` will not take part within the index.  A document
/// which does have both attributes `x` and `y` defined must have the values `0`
/// or `1` for attribute `x` and any value for attribute `y` will be acceptable,
/// otherwise the document will not be inserted within the collection.
///
/// @code
/// arangod> db.example.ensureBitarray("x", [0,1], "y", [2,"a",[]]);
/// {
///   "id" : "2755894/5770550",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1]], ["y", [2, "a", [ ]]]],
///   "undefined" : false,
///   "isNewlyCreated" : true
/// }
/// @endcode
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureBitarray = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }
  
  return this.ensureIndex({ 
    type: "bitarray", 
    fields: fields 
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
///
/// @FUN{@FA{collection}.ensureUndefBitarray(@FA{field1}, @FA{value1}, ..., @FA{fieldn}, @FA{valuen})}
///
/// Creates a bitarray index on all documents using attributes as paths to
/// the fields. At least one attribute and one set of possible values must be given.
/// All documents, which do not have the attribute path or
/// with one or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUndefBitarray = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  return this.ensureIndex({ 
    type: "bitarray", 
    fields: fields,
    "undefined": true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
///
/// @FUN{@FA{collection}.ensureCapConstraint(@FA{size}, {byteSize})}
///
/// Creates a size restriction aka cap for the collection of @FA{size}
/// documents and/or @FA{byteSize} data size. If the restriction is in place 
/// and the (@FA{size} plus one) document is added to the collection, or the
/// total active data size in the collection exceeds @FA{byteSize}, then the 
/// least recently created or updated documents are removed until all 
/// constraints are satisfied.
///
/// It is allowed to specify either @FA{size} or @FA{byteSize}, or both at
/// the same time. If both are specified, then the automatic document removal
/// will be triggered by the first non-met constraint.
///
/// Note that at most one cap constraint is allowed per collection.
///
/// Note that this does not imply any restriction of the number of revisions
/// of documents.
///
/// @EXAMPLES
///
/// Restrict the number of document to at most 10 documents:
///
/// @verbinclude ensure-cap-constraint
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
///
/// @FUN{ensureUniqueSkiplist(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
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
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueSkiplist = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }
  
  return this.ensureIndex({ 
    type: "skiplist", 
    fields: fields,
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a multi skiplist index exists
///
/// @FUN{ensureSkiplist(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
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
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureSkiplist = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }
  
  return this.ensureIndex({ 
    type: "skiplist", 
    fields: fields
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
///
/// @FUN{ensureFulltextIndex(@FA{field}, @FA{minWordLength})}
///
/// Creates a fulltext index on all documents on attribute @FA{field}.
/// All documents, which do not have the attribute @FA{field} or that have a
/// non-textual value inside their @FA{field} attribute are ignored.
///
/// The minimum length of words that are indexed can be specified with the
/// @FA{minWordLength} parameter. Words shorter than @FA{minWordLength}
/// characters will not be indexed. @FA{minWordLength} has a default value of 2,
/// but this value might be changed in future versions of ArangoDB. It is thus
/// recommended to explicitly specify this value
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude fulltext
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureFulltextIndex = function (fields, minLength) {
  "use strict";
  
  if (! Array.isArray(fields)) {
    fields = [ fields ];
  }

  this.ensureIndex({ 
    type: "fulltext", 
    minLength: minLength || undefined,
    fields: fields
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a unique constraint exists
///
/// @FUN{ensureUniqueConstraint(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a unique hash index on all documents using @FA{field1}, @FA{field2},
/// ... as attribute paths. At least one attribute path must be given.
///
/// When a unique constraint is in effect for a collection, then all documents
/// which contain the given attributes must differ in the attribute
/// values. Creating a new document or updating a document will fail, if the
/// uniqueness is violated. If any attribute value is null for a document, this
/// document is ignored by the index.
///
/// Note that non-existing attribute paths in a document are treated as if the
/// value were @LIT{null}.
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// @EXAMPLES
///
/// @verbinclude shell-index-create-unique-constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueConstraint = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }
  
  return this.ensureIndex({ 
    type: "hash", 
    fields: fields,
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
///
/// @FUN{ensureHashIndex(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a non-unique hash index on all documents using @FA{field1}, @FA{field2},
/// ... as attribute paths. At least one attribute path must be given.
///
/// Note that non-existing attribute paths in a document are treated as if the
/// value were @LIT{null}.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @EXAMPLES
///
/// @verbinclude shell-index-create-hash-index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureHashIndex = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }
  
  return this.ensureIndex({ 
    type: "hash", 
    fields: fields
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
///
/// @FUN{@FA{collection}.ensureGeoIndex(@FA{location})}
///
/// Creates a geo-spatial index on all documents using @FA{location} as path to
/// the coordinates. The value of the attribute must be a list with at least two
/// double values. The list must contain the latitude (first value) and the
/// longitude (second value). All documents, which do not have the attribute
/// path or with value that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// @FUN{@FA{collection}.ensureGeoIndex(@FA{location}, @LIT{true})}
///
/// As above which the exception, that the order within the list is longitude
/// followed by latitude. This corresponds to the format described in
///
/// http://geojson.org/geojson-spec.html#positions
///
/// @FUN{@FA{collection}.ensureGeoIndex(@FA{latitude}, @FA{longitude})}
///
/// Creates a geo-spatial index on all documents using @FA{latitude} and
/// @FA{longitude} as paths the latitude and the longitude. The value of the
/// attribute @FA{latitude} and of the attribute @FA{longitude} must a
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
///
/// @FUN{@FA{collection}.ensureGeoConstraint(@FA{location}, @FA{ignore-null})}
///
/// @FUN{@FA{collection}.ensureGeoConstraint(@FA{location}, @LIT{true}, @FA{ignore-null})}
///
/// @FUN{@FA{collection}.ensureGeoConstraint(@FA{latitude}, @FA{longitude}, @FA{ignore-null})}
///
/// Works like @FN{ensureGeoIndex} but requires that the documents contain
/// a valid geo definition. If @FA{ignore-null} is true, then documents with
/// a null in @FA{location} or at least one null in @FA{latitude} or
/// @FA{longitude} are ignored.
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

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  return this.lookupIndex({
    type: "hash",
    fields: fields,
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a hash index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupHashIndex = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  return this.lookupIndex({
    type: "hash",
    fields: fields
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a unique skiplist index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupUniqueSkiplist = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  return this.lookupIndex({
    type: "skiplist",
    fields: fields,
    unique: true
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a skiplist index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupSkiplist = function () {
  "use strict";

  var fields = [], i;

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  return this.lookupIndex({
    type: "skiplist",
    fields: fields
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a fulltext index
///
/// @FUN{lookupFulltextIndex(@FA{field}, @FA{minLength}}
///
/// Checks whether a fulltext index on the given attribute @FA{field} exists.
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupFulltextIndex = function (fields, minLength) {
  "use strict";

  if (! Array.isArray(fields)) {
    fields = [ fields ];
  }

  return this.lookupIndex({
    type: "fulltext",
    fields: fields,
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
