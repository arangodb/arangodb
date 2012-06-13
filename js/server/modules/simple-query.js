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
var SQ = require("simple-query-basics");

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

SQ.SimpleQueryAll.prototype.execute = function () {
  var documents;

  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    documents = this._collection.ALL(this._skip, this._limit);

    this._execution = new SQ.GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
}

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

function ByExample (collection, example, skip, limit) {
  var unique = true;
  var documentId = null;
  var attributes = [];

  for (var k in example) {
    if (example.hasOwnProperty(k)) {
      attributes.push(k);

      if (example[k] === null) {
        unique = false;
      }
      else if (k === '_id') {
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
    }
    catch (e) {
    }
    return { "total" : doc ? 1 : 0, "count" : doc ? 1 : 0, "documents" : doc ? [ doc ] : [ ] };
  }

  var idx = collection.lookupHashIndex.apply(collection, attributes);

  if (idx === null && unique) {
    idx = collection.lookupUniqueConstraint.apply(collection, attributes);

    if (idx !== null) {
      console.debug("found unique constraint %s", idx.id);
    }
  }
  else {
    console.debug("found hash index %s", idx.id);
  }

  if (idx !== null) {
    // use hash index
    return collection.BY_EXAMPLE_HASH(idx.id, example, skip, limit);
  }
  else {
    // use full collection scan
    return collection.BY_EXAMPLE(example, skip, limit);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryByExample.prototype.execute = function () {
  var documents;

  if (this._execution === null) {
    if (this._skip === null || this._skip <= 0) {
      this._skip = 0;
    }

    var documents = ByExample(this._collection, this._example, this._skip, this._limit);

    this._execution = new SQ.GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
}

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
/// @brief constructs a query-by-example for a collection
///
/// @FUN{@FA{collection}.firstExample(@FA{example})}
///
/// Returns the a document of a collection that match the specified example or
/// @LIT{null}. The example must be specified as paths and values. See 
/// @FN{byExample} for details.
///
/// @FUN{@FA{collection}.firstExample(@FA{path1}, @FA{value1}, ...)}
///
/// As alternative you can supply a list of paths and values.
///
/// @EXAMPLES
///
/// @TINYEXAMPLE{shell-simple-query-first-example,finds a document with a given name}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.firstExample = function () {
  var example;

  // example is given as only argument
  if (arguments.length === 1) {
    example = arguments[0];
  }

  // example is given as list
  else {
    example = {};

    for (var i = 0;  i < arguments.length;  i += 2) {
      example[arguments[i]] = arguments[i + 1];
    }
  }

  var documents = ByExample(this, example, 0, 1);

  if (0 < documents.documents.length) {
    return documents.documents[0];
  }
  else {
    return null;
  }
}

ArangoEdgesCollection.prototype.firstExample = ArangoCollection.prototype.firstExample;

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

function RangedQuery (collection, attribute, left, right, type, skip, limit) {
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
  else {
    throw "not implemented";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryRange.prototype.execute = function () {
  var documents;

  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    var documents = RangedQuery(this._collection,
                                this._attribute,
                                this._left,
                                this._right,
                                this._type,
                                this._skip, 
                                this._limit);

    this._execution = new SQ.GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
}

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
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryNear.prototype.execute = function () {
  var result;
  var documents;
  var distances;
  var limit;

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
      for (var i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new SQ.GeneralArrayCursor(result.documents, this._skip, null);
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
}

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
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryWithin.prototype.execute = function () {
  var result;
  var documents;
  var distances;
  var limit;

  if (this._execution === null) {
    result = this._collection.WITHIN(this._index, this._latitude, this._longitude, this._radius);
    documents = result.documents;
    distances = result.distances;

    if (this._distance !== null) {
      for (var i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new SQ.GeneralArrayCursor(result.documents, this._skip, this._limit);
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
}

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

exports.SimpleQueryAll = SQ.SimpleQueryAll;
exports.SimpleQueryByExample = SQ.SimpleQueryByExample;
exports.SimpleQueryGeo = SQ.SimpleQueryGeo;
exports.SimpleQueryNear = SQ.SimpleQueryNear;
exports.SimpleQueryWithin = SQ.SimpleQueryWithin;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
