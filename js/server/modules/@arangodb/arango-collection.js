/*jshint strict: false */
/*global ArangoClusterComm, require, exports, module */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoCollection
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2013 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
// / @brief builds an example query
// //////////////////////////////////////////////////////////////////////////////

function buildExampleQuery (collection, example, limit) {
  var parts = [];
  var bindVars = { '@collection': collection.name() };
  var keys = Object.keys(example);

  for (var i = 0; i < keys.length; ++i) {
    var key = keys[i];
    parts.push('doc.@att' + i + ' == @value' + i);
    bindVars['att' + i] = key;
    bindVars['value' + i] = example[key];
  }

  var query = 'FOR doc IN @@collection';
  if (parts.length > 0) {
    query += ' FILTER ' + parts.join(' && ', parts);
  }
  if (limit > 0) {
    query += ' LIMIT ' + parseInt(limit, 10);
  }

  return { query: query, bindVars: bindVars };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief add options from arguments to index specification
// //////////////////////////////////////////////////////////////////////////////

function addIndexOptions (body, parameters) {
  body.fields = [];

  var setOption = function (k) {
    if (! body.hasOwnProperty(k)) {
      body[k] = parameters[i][k];
    }
  };

  var i;
  for (i = 0; i < parameters.length; ++i) {
    if (typeof parameters[i] === 'string') {
      // set fields
      body.fields.push(parameters[i]);
    }
    else if (typeof parameters[i] === 'object' &&
      ! Array.isArray(parameters[i]) &&
      parameters[i] !== null) {
      // set arbitrary options
      Object.keys(parameters[i]).forEach(setOption);
      break;
    }
  }

  return body;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

var ArangoCollection = internal.ArangoCollection;
exports.ArangoCollection = ArangoCollection;

// must be called after exporting ArangoCollection
require('@arangodb/arango-collection-common');

var simple = require('@arangodb/simple-query');
var ArangoError = require('@arangodb').ArangoError;
var ArangoDatabase = require('@arangodb/arango-database').ArangoDatabase;

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionToArray
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toArray = function () {
  var cluster = require('@arangodb/cluster');

  if (cluster.isCoordinator()) {
    return this.all().toArray();
  }

  return this.ALL(null, null).documents;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionTruncate
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
  var cluster = require('@arangodb/cluster');
  if (cluster.isCoordinator()) {
    if (this.status() === ArangoCollection.STATUS_UNLOADED) {
      this.load();
    }
  }
  return this.TRUNCATE();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief finds an index of a collection
// /
// / @FUN{@FA{collection}.index(@FA{index-handle})}
// /
// / Returns the index with @FA{index-handle} or null if no such index exists.
// /
// / *Examples*
// /
// / @code
// / arango> db.example.getIndexes().map(function(x) { return x.id; })
// / ["example/0"]
// / arango> db.example.index("93013/0")
// / { "id" : "example/0", "type" : "primary", "fields" : ["_id"] }
// / @endcode
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.index = function (id) {
  var indexes = this.getIndexes();
  var i;

  if (typeof id === 'object' && id.hasOwnProperty('id')) {
    id = id.id;
  }

  if (typeof id === 'string') {
    var pa = ArangoDatabase.indexRegex.exec(id);

    if (pa === null) {
      id = this.name() + '/' + id;
    }
  }
  else if (typeof id === 'number') {
    // stringify the id
    id = this.name() + '/' + id;
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns connected edges
// //////////////////////////////////////////////////////////////////////////////

function getEdges (collection, vertex, direction) {
  if (direction === 'in') {
    return collection.INEDGES(vertex);
  }
  if (direction === 'out') {
    return collection.OUTEDGES(vertex);
  }

  return collection.EDGES(vertex);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionEdgesAll
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.edges = function (vertex) {
  return getEdges(this, vertex, 'any');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionEdgesInbound
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.inEdges = function (vertex) {
  return getEdges(this, vertex, 'in');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionEdgesOutbound
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.outEdges = function (vertex) {
  return getEdges(this, vertex, 'out');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock documentsCollectionAny
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.any = function () {
  var cluster = require('@arangodb/cluster');

  if (cluster.isCoordinator()) {
    var dbName = require('internal').db._name();
    var shards = cluster.shardList(dbName, this.name());
    var coord = { coordTransactionID: ArangoClusterComm.getId() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };

    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest('put',
        'shard:' + shard,
        dbName,
        '/_api/simple/any',
        JSON.stringify({
          collection: shard
        }),
        { },
        options);
    });

    var results = cluster.wait(coord, shards.length), i;
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionFirstExample
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief removes documents matching an example
// / @param example a json object which is used as sample for finding docs that
// /        this example matches
// / @param waitForSync (optional) a boolean value or a json object
// / @param limit (optional) an integer value, the max number of to deleting docs
// / @example remove("{a : 1 }")
// / @example remove("{a : 1 }", true)
// / @example remove("{a : 1 }", false)
// / @example remove("{a : 1 }", {waitForSync: false, limit: 12})
// / @throws "too many parameters" when waiitForSyn is a Json object and limit
// /         is given
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example,
  waitForSync,
  limit) {
  if (limit === 0) {
    return 0;
  }

  if (typeof waitForSync === 'object') {
    if (typeof limit !== 'undefined') {
      throw 'too many parameters';
    }
    var tmp_options = waitForSync === null ? {} : waitForSync;
    // avoiding jslint error
    // see: http://jslinterrors.com/unexpected-sync-method-a/
    waitForSync = tmp_options.waitForSync;
    limit = tmp_options.limit;
  }

  var i;
  var cluster = require('@arangodb/cluster');

  var query = buildExampleQuery(this, example, limit);
  var opts = { waitForSync: waitForSync };
  query.query += ' REMOVE doc IN @@collection OPTIONS ' + JSON.stringify(opts);

  return require('internal').db._query(query).getExtra().stats.writesExecuted;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief replaces documents matching an example
// / @param example the json object for finding documents which matches this
// /        example
// / @param newValue the json object which replaces the matched documents
// / @param waitForSync (optional) a boolean value or a json object which
// /        contains the options
// / @param limit (optional) an integer value, the max number of to deleting docs
// / @throws "too many parameters" when waitForSync is a json object and limit
// /        was given
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example,
  newValue,
  waitForSync,
  limit) {
  if (limit === 0) {
    return 0;
  }

  if (typeof newValue !== 'object' || Array.isArray(newValue)) {
    var err1 = new ArangoError();
    err1.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err1.errorMessage = "invalid value for parameter 'newValue'";

    throw err1;
  }

  if (typeof waitForSync === 'object') {
    if (typeof limit !== 'undefined') {
      throw 'too many parameters';
    }
    var tmp_options = waitForSync === null ? {} : waitForSync;
    // avoiding jslint error
    // see: http://jslinterrors.com/unexpected-sync-method-a/
    waitForSync = tmp_options.waitForSync;
    limit = tmp_options.limit;
  }

  var query = buildExampleQuery(this, example, limit);
  var opts = { waitForSync: waitForSync };
  query.query += ' REPLACE doc WITH @newValue IN @@collection OPTIONS ' + JSON.stringify(opts);
  query.bindVars.newValue = newValue;

  return require('internal').db._query(query).getExtra().stats.writesExecuted;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief partially updates documents matching an example
// / @param example the json object for finding documents which matches this
// /        example
// / @param newValue the json object which replaces the matched documents
// / @param keepNull (optional) true or a JSON object which
// /        contains the options
// / @param waitForSync (optional) a boolean value
// / @param limit (optional) an integer value, the max number of to deleting docs
// / @throws "too many parameters" when keepNull is a JSON object and limit
// /        was given
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example,
  newValue,
  keepNull,
  waitForSync,
  limit) {
  if (limit === 0) {
    return 0;
  }

  if (typeof newValue !== 'object' || Array.isArray(newValue)) {
    var err1 = new ArangoError();
    err1.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err1.errorMessage = "invalid value for parameter 'newValue'";

    throw err1;
  }

  var mergeObjects = true;

  if (typeof keepNull === 'object') {
    if (typeof waitForSync !== 'undefined') {
      throw 'too many parameters';
    }
    var tmp_options = keepNull === null ? {} : keepNull;

    // avoiding jslint error
    // see: http://jslinterrors.com/unexpected-sync-method-a/
    keepNull = tmp_options.keepNull;
    waitForSync = tmp_options.waitForSync;
    limit = tmp_options.limit;
    if (tmp_options.hasOwnProperty('mergeObjects')) {
      mergeObjects = tmp_options.mergeObjects || false;
    }
  }

  var query = buildExampleQuery(this, example, limit);
  var opts = { waitForSync: waitForSync, keepNull: keepNull, mergeObjects: mergeObjects };
  query.query += ' UPDATE doc WITH @newValue IN @@collection OPTIONS ' + JSON.stringify(opts);
  query.bindVars.newValue = newValue;

  return require('internal').db._query(query).getExtra().stats.writesExecuted;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief looks up a unique constraint
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupUniqueConstraint = function () {
  'use strict';

  return this.lookupIndex({
    type: 'hash',
    fields: Array.prototype.slice.call(arguments),
    unique: true
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief looks up a hash index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupHashIndex = function () {
  'use strict';

  return this.lookupIndex({
    type: 'hash',
    fields: Array.prototype.slice.call(arguments)
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief looks up a unique skiplist index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupUniqueSkiplist = function () {
  'use strict';

  return this.lookupIndex({
    type: 'skiplist',
    fields: Array.prototype.slice.call(arguments),
    unique: true
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief looks up a skiplist index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupSkiplist = function () {
  'use strict';

  return this.lookupIndex({
    type: 'skiplist',
    fields: Array.prototype.slice.call(arguments)
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock lookUpFulltextIndex
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.lookupFulltextIndex = function (field, minLength) {
  'use strict';

  if (! Array.isArray(field)) {
    field = [ field ];
  }

  return this.lookupIndex({
    type: 'fulltext',
    fields: field,
    minLength: minLength || undefined
  });
};
