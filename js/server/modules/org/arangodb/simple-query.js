/*jshint strict: false */
/*global require, exports, ArangoClusterComm, ArangoClusterInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief Arango Simple Query Language
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var console = require("console");

var ArangoError = require("org/arangodb").ArangoError;

var sq = require("org/arangodb/simple-query-common");

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryArray = sq.SimpleQueryArray;
var SimpleQueryByExample = sq.SimpleQueryByExample;
var SimpleQueryByCondition = sq.SimpleQueryByCondition;
var SimpleQueryFulltext = sq.SimpleQueryFulltext;
var SimpleQueryGeo = sq.SimpleQueryGeo;
var SimpleQueryNear = sq.SimpleQueryNear;
var SimpleQueryRange = sq.SimpleQueryRange;
var SimpleQueryWithin = sq.SimpleQueryWithin;

////////////////////////////////////////////////////////////////////////////////
/// @brief rewrites an index id by stripping the collection name from it
////////////////////////////////////////////////////////////////////////////////

var rewriteIndex = function (id) {
  if (id === null || id === undefined) {
    return;
  }

  if (typeof id === "string") {
    return id.replace(/^[a-zA-Z0-9_\-]+\//, '');
  }
  if (typeof id === "object" && id.hasOwnProperty("id")) {
    return id.id.replace(/^[a-zA-Z0-9_\-]+\//, '');
  }
  return id;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY ALL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.execute = function () {
  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    var documents;
    var cluster = require("org/arangodb/cluster");

    if (cluster.isCoordinator()) {
      var dbName = require("internal").db._name();
      var shards = cluster.shardList(dbName, this._collection.name());
      var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
      var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
      var limit = 0;
      if (this._limit > 0) {
        if (this._skip >= 0) {
          limit = this._skip + this._limit;
        }
      }

      shards.forEach(function (shard) {
        ArangoClusterComm.asyncRequest("put",
                                       "shard:" + shard,
                                       dbName,
                                       "/_api/simple/all",
                                       JSON.stringify({
                                         collection: shard,
                                         skip: 0,
                                         limit: limit || undefined,
                                         batchSize: 100000000
                                       }),
                                       { },
                                       options);
      });

      var _documents = [ ], total = 0;
      var result = cluster.wait(coord, shards);
      var toSkip = this._skip, toLimit = this._limit;

      if (toSkip < 0) {
        // negative skip is special
        toLimit = null;
      }

      result.forEach(function(part) {
        var body = JSON.parse(part.body);
        total += body.count;

        if (toSkip > 0) {
          if (toSkip >= body.result.length) {
            toSkip -= body.result.length;
            return;
          }

          body.result = body.result.slice(toSkip);
          toSkip = 0;
        }

        if (toLimit !== null && toLimit !== undefined) {
          if (body.result.length >= toLimit) {
            body.result = body.result.slice(0, toLimit);
            toLimit = 0;
          }
          else {
            toLimit -= body.result.length;
          }
        }

        _documents = _documents.concat(body.result);
      });

      if (this._skip < 0) {
        // apply negative skip
        var start = _documents.length + this._skip;
        _documents = _documents.slice(start, start + (this._limit || 100000000));
      }

      documents = {
        documents: _documents,
        count: _documents.length,
        total: total
      };
    }
    else {
      documents = this._collection.ALL(this._skip, this._limit);
    }

    this._execution = new GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY BY EXAMPLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalise attribute names for searching in indexes
/// this will turn
/// { a: { b: { c: 1, d: true }, e: "bar" } }
/// into
/// { "a.b.c" : 1, "a.b.d" : true, "a.e" : "bar" }
////////////////////////////////////////////////////////////////////////////////

function normalizeAttributes (obj, prefix) {
  var prep = ((prefix === "" || prefix === undefined) ? "" : prefix + "."), o, normalized = { };

  for (o in obj) {
    if (obj.hasOwnProperty(o)) {
      if (typeof obj[o] === 'object' && ! Array.isArray(obj[o]) && obj[o] !== null) {
        var sub = normalizeAttributes(obj[o], prep + o), i;
        for (i in sub) {
          if (sub.hasOwnProperty(i)) {
            normalized[i] = sub[i];
          }
        }
      }
      else {
        normalized[prep + o] = obj[o];
      }
    }
  }

  return normalized;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not an index supports a query
////////////////////////////////////////////////////////////////////////////////

function supportsQuery (idx, attributes) {
  var i, n;
  var fields;

  fields = idx.fields;

  n = fields.length;
  for (i = 0; i < n; ++i) {
    var field = fields[i];
    if (attributes.indexOf(field) === -1) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a document attribute
////////////////////////////////////////////////////////////////////////////////

function docType (value) {
  "use strict";

  if (value !== undefined && value !== null) {
    if (Array.isArray(value)) {
      return 'array';
    }

    switch (typeof(value)) {
      case 'boolean':
        return 'boolean';
      case 'number':
        if (isNaN(value) || ! isFinite(value)) {
          // not a number => undefined
          return 'null';
        }
        return 'number';
      case 'string':
        return 'string';
      case 'object':
        return 'object';
    }
  }

  return 'null';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether or not the example is contained in the document
////////////////////////////////////////////////////////////////////////////////

function isContained (doc, example) {
  "use strict";

  var eType = docType(example);
  var dType = docType(doc);
  if (eType !== dType) {
    return false;
  }
     
  var i; 
  if (eType === 'object') {
    for (i in example) {
      if (example.hasOwnProperty(i)) {
        if (! doc.hasOwnProperty(i) ||
            ! isContained(doc[i], example[i])) {
          return false;
        }
      }
    }
  }
  else if (eType === 'array') {
    if (doc.length !== example.length) {
      return false;
    }
    for (i = 0; i < doc.length; ++i) {
      if (! isContained(doc[i], example[i])) {
        return false;
      }
    }
  }
  else if (doc !== example) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a unique index can be used
////////////////////////////////////////////////////////////////////////////////

function isUnique (example) {
  var k;
  for (k in example) {
    if (example.hasOwnProperty(k)) {
      if (example[k] === null) {
        return false;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds documents by example
////////////////////////////////////////////////////////////////////////////////

function byExample (data) {
  var k;

  var collection = data._collection;
  var example    = data._example;
  var skip       = data._skip;
  var limit      = data._limit;

  if (typeof example !== "object" || Array.isArray(example)) {
    // invalid datatype for example
    var err1 = new ArangoError();
    err1.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code;
    err1.errorMessage = "invalid document type '" + (typeof example) + "'";
    throw err1;
  }

  var postFilter = false;
  if (example.hasOwnProperty('_rev')) {
    // the presence of the _rev attribute requires post-filtering
    postFilter = true;
  }

  var candidates = { total: 0, count: 0, documents: [ ] };
  if (example.hasOwnProperty('_id')) {
    // we can use the collection's primary index
    try {
      candidates.documents = [ collection.document(example._id) ];
      postFilter = true;
    }
    catch (n1) {
    }
  }
  else if (example.hasOwnProperty('_key')) {
    // we can use the collection's primary index
    try {
      candidates.documents = [ collection.document(example._key) ];
      postFilter = true;
    }
    catch (n2) {
    }
  }
  else if (example.hasOwnProperty('_from')) {
    // use edge index
    try {
      candidates.documents = collection.outEdges(example._from);
      postFilter = true;
    }
    catch (n3) {
    }
  }
  else if (example.hasOwnProperty('_to')) {
    // use edge index
    try {
      candidates.documents = collection.inEdges(example._to);
      postFilter = true;
    }
    catch (n4) {
    }
  }
  else {
    // check indexes
    var idx = null;
    var normalized = normalizeAttributes(example, "");
    var keys = Object.keys(normalized);
    var index;

    if (data._index !== undefined && data._index !== null) {
      if (typeof data._index === 'object' && data._index.hasOwnProperty("id")) {
        index = data._index.id;
      }
      else if (typeof data._index === 'string') {
        index = data._index;
      }
    }

    if (index !== undefined) {
      // an index was specified
      var all = collection.getIndexes();
      for (k = 0; k < all.length; ++k) {
        if (all[k].type === data._type &&
            rewriteIndex(all[k].id) === rewriteIndex(index)) {

          if (supportsQuery(all[k], keys)) {
            idx = all[k];
          }
          break;
        }
      }
    }
    else if (keys.length > 0) {
      // try these index types
      var checks = [
        { type: "hash", fields: keys, unique: false },
        { type: "skiplist", fields: keys, unique: false }
      ];

      if (isUnique(example)) {
        checks.push({ type: "hash", fields: keys, unique: true });
        checks.push({ type: "skiplist", fields: keys, unique: true });
      }

      var fields = [ ];
      for (k in normalized) {
        if (normalized.hasOwnProperty(k)) {
          fields.push([ k, [ normalized[k] ] ]);
        }
      }

      for (k = 0; k < checks.length; ++k) {
        if (data._type !== undefined && data._type !== checks[k].type) {
          continue;
        }

        idx = collection.lookupIndex(checks[k]);
        if (idx !== null) {
          // found an index
          break;
        }
      }
    }
  
    if (idx !== null) {
      // use an index
      if (idx.type === 'hash') {
        candidates = collection.BY_EXAMPLE_HASH(idx.id, normalized, skip, limit);
      }
      else if (idx.type === 'skiplist') {
        candidates = collection.BY_EXAMPLE_SKIPLIST(idx.id, normalized, skip, limit);
      }
    }
    else {
      if (typeof data._type === "string") {
        // an index was specified, but none can be used
        var err2 = new ArangoError();
        err2.errorNum = internal.errors.ERROR_ARANGO_NO_INDEX.code;
        err2.errorMessage = internal.errors.ERROR_ARANGO_NO_INDEX.message;
        throw err2;
      }

      if (postFilter) {
        candidates = collection.ALL(skip, limit);
      }
      else {
        candidates = collection.BY_EXAMPLE(example, skip, limit);
      }
    }
  }
    
  if (postFilter) {
    var result = [ ];

    for (k = 0; k < candidates.documents.length; ++k) {
      var doc = candidates.documents[k];
      if (! isContained(doc, example)) {
        continue;
      }

      if (skip > 0) {
        --skip;
        continue;
      }

      result.push(doc);
      if (limit > 0 && result.length >= limit) {
        break;
      }
    }
    
    candidates.total = candidates.documents.length;
    candidates.count = result.length;
    candidates.documents = result;
  }

  return candidates;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.execute = function () {
  var documents;

  if (this._execution === null) {
    if (this._skip === null || this._skip <= 0) {
      this._skip = 0;
    }

    var cluster = require("org/arangodb/cluster");

    if (cluster.isCoordinator()) {
      var dbName = require("internal").db._name();
      var shards = cluster.shardList(dbName, this._collection.name());
      var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
      var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
      var limit = 0;
      if (this._limit > 0) {
        if (this._skip >= 0) {
          limit = this._skip + this._limit;
        }
      }

      var method = "by-example";
      if (this._type !== undefined) {
        switch (this._type) {
          case "hash":
            method = "by-example-hash";
            break;
          case "skiplist":
            method = "by-example-skiplist";
            break;
        }
      }

      var self = this;
      shards.forEach(function (shard) {
        ArangoClusterComm.asyncRequest("put",
                                       "shard:" + shard,
                                       dbName,
                                       "/_api/simple/" + method,
                                       JSON.stringify({
                                         example: self._example,
                                         collection: shard,
                                         skip: 0,
                                         limit: limit || undefined,
                                         batchSize: 100000000,
                                         index: rewriteIndex(self._index)
                                       }),
                                       { },
                                       options);
      });

      var _documents = [ ], total = 0;
      var result = cluster.wait(coord, shards);
      var toSkip = this._skip, toLimit = this._limit;

      if (toSkip < 0) {
        // negative skip is special
        toLimit = null;
      }

      result.forEach(function(part) {
        var body = JSON.parse(part.body);
        total += body.count;

        if (toSkip > 0) {
          if (toSkip >= body.result.length) {
            toSkip -= body.result.length;
            return;
          }

          body.result = body.result.slice(toSkip);
          toSkip = 0;
        }

        if (toLimit !== null && toLimit !== undefined) {
          if (body.result.length >= toLimit) {
            body.result = body.result.slice(0, toLimit);
            toLimit = 0;
          }
          else {
            toLimit -= body.result.length;
          }
        }

        _documents = _documents.concat(body.result);
      });

      if (this._skip < 0) {
        // apply negative skip
        var start = _documents.length + this._skip;
        _documents = _documents.slice(start, start + (this._limit || 100000000));
      }

      documents = {
        documents: _documents,
        count: _documents.length,
        total: total
      };
    }
    else {
      documents = byExample(this);
    }

    this._execution = new GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief query by condition
////////////////////////////////////////////////////////////////////////////////

function byCondition (data) {
  var collection = data._collection;
  var condition  = data._condition;
  var skip       = data._skip;
  var limit      = data._limit;
  var index;

  if (data._index !== undefined && data._index !== null) {
    if (typeof data._index === 'object' && data._index.hasOwnProperty("id")) {
      index = data._index.id;
    }
    else if (typeof data._index === 'string') {
      index = data._index;
    }
  }
  else {
    var err1 = new ArangoError();
    err1.errorNum = internal.errors.ERROR_ARANGO_NO_INDEX.code;
    err1.errorMessage = internal.errors.ERROR_ARANGO_NO_INDEX.message;
    throw err1;
  }

  if (typeof condition !== "object" || Array.isArray(condition)) {
    // invalid datatype for condition
    var err2 = new ArangoError();
    err2.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    err2.errorMessage = "invalid document type";
    throw err2;
  }

  // always se an index
  switch (data._type) {
    case "skiplist":
      return collection.BY_CONDITION_SKIPLIST(index, condition, skip, limit);
  }

  // an index type is required, but no index will be used
  var err3 = new ArangoError();
  err3.errorNum = internal.errors.ERROR_ARANGO_NO_INDEX.code;
  err3.errorMessage = internal.errors.ERROR_ARANGO_NO_INDEX.message;
  throw err3;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-condition
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype.execute = function () {
  var documents;

  if (this._execution === null) {
    if (this._skip === null || this._skip <= 0) {
      this._skip = 0;
    }

    var cluster = require("org/arangodb/cluster");

    if (cluster.isCoordinator()) {
      var dbName = require("internal").db._name();
      var shards = cluster.shardList(dbName, this._collection.name());
      var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
      var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
      var limit = 0;
      if (this._limit > 0) {
        if (this._skip >= 0) {
          limit = this._skip + this._limit;
        }
      }

      var method;
      if (this._type !== undefined) {
        switch (this._type) {
          case "skiplist":
            method = "by-condition-skiplist";
            break;
        }
      }

      var self = this;
      shards.forEach(function (shard) {
        ArangoClusterComm.asyncRequest("put",
                                       "shard:" + shard,
                                       dbName,
                                       "/_api/simple/" + method,
                                       JSON.stringify({
                                         condition: self._condition,
                                         collection: shard,
                                         skip: 0,
                                         limit: limit || undefined,
                                         batchSize: 100000000,
                                         index: rewriteIndex(self._index)
                                       }),
                                       { },
                                       options);
      });

      var _documents = [ ], total = 0;
      var result = cluster.wait(coord, shards);
      var toSkip = this._skip, toLimit = this._limit;

      if (toSkip < 0) {
        // negative skip is special
        toLimit = null;
      }

      result.forEach(function(part) {
        var body = JSON.parse(part.body);
        total += body.count;

        if (toSkip > 0) {
          if (toSkip >= body.result.length) {
            toSkip -= body.result.length;
            return;
          }

          body.result = body.result.slice(toSkip);
          toSkip = 0;
        }

        if (toLimit !== null && toLimit !== undefined) {
          if (body.result.length >= toLimit) {
            body.result = body.result.slice(0, toLimit);
            toLimit = 0;
          }
          else {
            toLimit -= body.result.length;
          }
        }

        _documents = _documents.concat(body.result);
      });

      if (this._skip < 0) {
        // apply negative skip
        var start = _documents.length + this._skip;
        _documents = _documents.slice(start, start + (this._limit || 100000000));
      }

      documents = {
        documents: _documents,
        count: _documents.length,
        total: total
      };
    }
    else {
      documents = byCondition(this);
    }

    this._execution = new GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      RANGED QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ranged query
////////////////////////////////////////////////////////////////////////////////

function rangedQuery (collection, attribute, left, right, type, skip, limit) {
  var documents;
  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, collection.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
    var _limit = 0;
    if (limit > 0) {
      if (skip >= 0) {
        _limit = skip + limit;
      }
    }

    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/range",
                                     JSON.stringify({
                                       collection: shard,
                                       attribute: attribute,
                                       left: left,
                                       right: right,
                                       closed: type,
                                       skip: 0,
                                       limit: _limit || undefined,
                                       batchSize: 100000000
                                     }),
                                     { },
                                     options);
    });

    var _documents = [ ], total = 0;
    var result = cluster.wait(coord, shards);

    result.forEach(function(part) {
      var body = JSON.parse(part.body);
      total += body.count;

      _documents = _documents.concat(body.result);
    });

    if (shards.length > 1) {
      var cmp = require("org/arangodb/ahuacatl").RELATIONAL_CMP;
      _documents.sort(function (l, r) {
        return cmp(l[attribute], r[attribute]);
      });
    }

    if (limit > 0 && skip >= 0) {
      _documents = _documents.slice(skip, skip + limit);
    }
    else if (skip > 0) {
      _documents = _documents.slice(skip, _documents.length);
    }
    else if (skip < 0) {
      // apply negative skip
      var start = _documents.length + skip;
      _documents = _documents.slice(start, start + (limit || 100000000));
    }

    documents = {
      documents: _documents,
      count: _documents.length,
      total: total
    };
  }
  else {
    var idx = collection.lookupSkiplist(attribute);

    if (idx === null) {
      idx = collection.lookupUniqueSkiplist(attribute);

      if (idx !== null) {
        console.debug("found unique skip-list index %s", idx.id);
      }
    }
    else {
      console.debug("found skip-list index %s", idx.id);
    }

    if (idx !== null) {
      var cond = {};

      if (type === 0) {
        cond[attribute] = [ [ ">=", left ], [ "<", right ] ];
      }
      else if (type === 1) {
        cond[attribute] = [ [ ">=", left ], [ "<=", right ] ];
      }
      else {
        throw "unknown type";
      }

      documents = collection.BY_CONDITION_SKIPLIST(idx.id, cond, skip, limit);
    }
    else {
      throw "not implemented";
    }
  }

  return documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a range query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.execute = function () {
  var documents;

  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    documents = rangedQuery(this._collection,
                            this._attribute,
                            this._left,
                            this._right,
                            this._type,
                            this._skip,
                            this._limit);

    this._execution = new GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE QUERY NEAR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.execute = function () {
  var documents;
  var result;
  var limit;
  var i, n;

  if (this._execution !== null) {
    return;
  }

  if (this._skip === null) {
    this._skip = 0;
  }

  if (this._skip < 0) {
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_BAD_PARAMETER;
    err.errorMessage = "skip must be non-negative";
    throw err;
  }

  if (this._limit === null) {
    limit = this._skip + 100;
  }
  else {
    limit = this._skip + this._limit;
  }

  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this._collection.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
    var _limit = 0;
    if (this._limit > 0) {
      if (this._skip >= 0) {
        _limit = this._skip + this._limit;
      }
    }

    var attribute;
    if (this._distance !== null) {
      attribute = this._distance;
    }
    else {
      // use a pseudo-attribute for distance (we need this for sorting)
      attribute = "$distance";
    }

    var self = this;
    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/near",
                                     JSON.stringify({
                                       collection: shard,
                                       latitude: self._latitude,
                                       longitude: self._longitude,
                                       distance: attribute,
                                       geo: rewriteIndex(self._index),
                                       skip: 0,
                                       limit: _limit || undefined,
                                       batchSize: 100000000
                                     }),
                                     { },
                                     options);
    });

    var _documents = [ ], total = 0;
    result = cluster.wait(coord, shards);

    result.forEach(function(part) {
      var body = JSON.parse(part.body);
      total += body.total;

      _documents = _documents.concat(body.result);
    });

    if (shards.length > 1) {
      _documents.sort(function (l, r) {
        if (l[attribute] === r[attribute]) {
          return 0;
        }
        return (l[attribute] < r[attribute] ? -1 : 1);
      });
    }

    if (this._limit > 0) {
      _documents = _documents.slice(0, this._skip + this._limit);
    }

    if (this._distance === null) {
      n = _documents.length;
      for (i = 0; i < n; ++i) {
        delete _documents[i][attribute];
      }
    }

    documents = {
      documents: _documents,
      count: _documents.length,
      total: total
    };
  }
  else {
    result = this._collection.NEAR(this._index, this._latitude, this._longitude, limit);

    documents = {
      documents: result.documents,
      count: result.documents.length,
      total: result.documents.length
    };

    if (this._distance !== null) {
      var distances = result.distances;
      n = documents.documents.length;
      for (i = this._skip;  i < n;  ++i) {
        documents.documents[i][this._distance] = distances[i];
      }
    }
  }

  this._execution = new GeneralArrayCursor(documents.documents, this._skip, null);
  this._countQuery = documents.total - this._skip;
  this._countTotal = documents.total;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               SIMPLE QUERY WITHIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.execute = function () {
  var result;
  var documents;
  var i, n;

  if (this._execution !== null) {
    return;
  }

  if (this._skip === null) {
    this._skip = 0;
  }

  if (this._skip < 0) {
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_BAD_PARAMETER;
    err.errorMessage = "skip must be non-negative";
    throw err;
  }

  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this._collection.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
    var _limit = 0;
    if (this._limit > 0) {
      if (this._skip >= 0) {
        _limit = this._skip + this._limit;
      }
    }

    var attribute;
    if (this._distance !== null) {
      attribute = this._distance;
    }
    else {
      // use a pseudo-attribute for distance (we need this for sorting)
      attribute = "$distance";
    }

    var self = this;
    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/within",
                                     JSON.stringify({
                                       collection: shard,
                                       latitude: self._latitude,
                                       longitude: self._longitude,
                                       distance: attribute,
                                       radius: self._radius,
                                       geo: rewriteIndex(self._index),
                                       skip: 0,
                                       limit: _limit || undefined,
                                       batchSize: 100000000
                                     }),
                                     { },
                                     options);
    });

    var _documents = [ ], total = 0;
    result = cluster.wait(coord, shards);

    result.forEach(function(part) {
      var body = JSON.parse(part.body);
      total += body.total;

      _documents = _documents.concat(body.result);
    });

    if (shards.length > 1) {
      _documents.sort(function (l, r) {
        if (l[attribute] === r[attribute]) {
          return 0;
        }
        return (l[attribute] < r[attribute] ? -1 : 1);
      });
    }

    if (this._limit > 0) {
      _documents = _documents.slice(0, this._skip + this._limit);
    }

    if (this._distance === null) {
      n = _documents.length;
      for (i = 0; i < n; ++i) {
        delete _documents[i][attribute];
      }
    }

    documents = {
      documents: _documents,
      count: _documents.length,
      total: total
    };
  }
  else {
    result = this._collection.WITHIN(this._index, this._latitude, this._longitude, this._radius);

    documents = {
      documents: result.documents,
      count: result.documents.length,
      total: result.documents.length
    };

    if (this._limit > 0) {
      documents.documents = documents.documents.slice(0, this._skip + this._limit);
    }

    if (this._distance !== null) {
      var distances = result.distances;
      n = documents.documents.length;
      for (i = this._skip;  i < n;  ++i) {
        documents.documents[i][this._distance] = distances[i];
      }
    }
  }

  this._execution = new GeneralArrayCursor(documents.documents, this._skip, null);
  this._countQuery = documents.total - this._skip;
  this._countTotal = documents.total;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             SIMPLE QUERY FULLTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a fulltext query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.execute = function () {
  var result;
  var documents;

  if (this._execution !== null) {
    return;
  }

  var cluster = require("org/arangodb/cluster");

  if (cluster.isCoordinator()) {
    var dbName = require("internal").db._name();
    var shards = cluster.shardList(dbName, this._collection.name());
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout: 360 };
    var _limit = 0;
    if (this._limit > 0) {
      if (this._skip >= 0) {
        _limit = this._skip + this._limit;
      }
    }

    var self = this;
    shards.forEach(function (shard) {
      ArangoClusterComm.asyncRequest("put",
                                     "shard:" + shard,
                                     dbName,
                                     "/_api/simple/fulltext",
                                     JSON.stringify({
                                       collection: shard,
                                       attribute: self._attribute,
                                       query: self._query,
                                       index: rewriteIndex(self._index),
                                       skip: 0,
                                       limit: _limit || undefined,
                                       batchSize: 100000000
                                     }),
                                     { },
                                     options);
    });

    var _documents = [ ], total = 0;
    result = cluster.wait(coord, shards);

    result.forEach(function(part) {
      var body = JSON.parse(part.body);
      total += body.total;

      _documents = _documents.concat(body.result);
    });

    if (this._limit > 0) {
      _documents = _documents.slice(0, this._skip + this._limit);
    }

    documents = {
      documents: _documents,
      count: _documents.length,
      total: total
    };
  }
  else {
    result = this._collection.FULLTEXT(this._index, this._query);

    documents = {
      documents: result.documents,
      count: result.documents.length - this._skip,
      total: result.documents.length
    };

    if (this._limit > 0) {
      documents.documents = documents.documents.slice(0, this._skip + this._limit);
    }
  }

  this._execution = new GeneralArrayCursor(documents.documents, this._skip, null);
  this._countQuery = documents.total - this._skip;
  this._countTotal = documents.total;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryByCondition = SimpleQueryByCondition;
exports.SimpleQueryFulltext = SimpleQueryFulltext;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.byExample = byExample;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
