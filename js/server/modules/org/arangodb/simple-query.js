/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

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

var ArangoError = require("org/arangodb/arango-error");

var sq = require("org/arangodb/simple-query-common");

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryArray = sq.SimpleQueryArray;
var SimpleQueryByExample = sq.SimpleQueryByExample;
var SimpleQueryFulltext = sq.SimpleQueryFulltext;
var SimpleQueryGeo = sq.SimpleQueryGeo;
var SimpleQueryNear = sq.SimpleQueryNear;
var SimpleQueryRange = sq.SimpleQueryRange;
var SimpleQueryWithin = sq.SimpleQueryWithin;

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
  var documents;

  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    documents = this._collection.ALL(this._skip, this._limit);

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
/// @brief query-by scan or hash index
////////////////////////////////////////////////////////////////////////////////

function byExample (collection, example, skip, limit) {
  var unique = true;
  var documentId = null;
  var attributes = [];
  var k;

  for (k in example) {
    if (example.hasOwnProperty(k)) {
      attributes.push(k);

      if (example[k] === null) {
        unique = false;
      }
      else if (k === '_id' || k === '_key') {
        // example contains the document id in attribute "_id"
        documentId = example[k];
        break;
      }
    }
  }

  if (documentId !== null) {
    // we can use the collection's primary index
    var doc;
    try {
      // look up document by id
      doc = collection.document(documentId);
    
      // we have used the primary index to look up the document
      // now we need to post-filter because non-indexed values might not have matched  
      for (k in example) {
        if (example.hasOwnProperty(k)) {
          if (doc[k] !== example[k]) {
            doc = null;
            break;
          }
        }
      }
    }
    catch (e) {
    }

    return { "total" : doc ? 1 : 0, "count" : doc ? 1 : 0, "documents" : doc ? [ doc ] : [ ] };
  }

  var idx = null;

  try {
    idx = collection.lookupHashIndex.apply(collection, attributes);
    if (idx === null && unique) {
      idx = collection.lookupUniqueConstraint.apply(collection, attributes);

      if (idx !== null) {
        console.debug("found unique constraint %s", idx.id);
      }
    }
    else if (idx !== null) {
      console.debug("found hash index %s", idx.id);
    }
  }
  catch (err) {
  }

  if (idx !== null) {
    // use hash index
    return collection.BY_EXAMPLE_HASH(idx.id, example, skip, limit);
  }

  // use full collection scan
  return collection.BY_EXAMPLE(example, skip, limit);
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

    documents = byExample(this._collection, this._example, this._skip, this._limit);

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

    return collection.BY_CONDITION_SKIPLIST(idx.id, cond, skip, limit);
  }

  throw "not implemented";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
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
  var result;
  var documents;
  var distances;
  var limit;
  var i;

  if (this._execution === null) {
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

    result = this._collection.NEAR(this._index, this._latitude, this._longitude, limit);
    documents = result.documents;
    distances = result.distances;

    if (this._distance !== null) {
      for (i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new GeneralArrayCursor(result.documents, this._skip, null);
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
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
  var distances;
  var i;

  if (this._execution === null) {
    result = this._collection.WITHIN(this._index, this._latitude, this._longitude, this._radius);
    documents = result.documents;
    distances = result.distances;

    if (this._distance !== null) {
      for (i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new GeneralArrayCursor(result.documents, this._skip, this._limit);
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
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

  if (this._execution === null) {
    result = this._collection.FULLTEXT(this._index, this._query);
    documents = result.documents;

    this._execution = new GeneralArrayCursor(result.documents, this._skip, this._limit);
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
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
