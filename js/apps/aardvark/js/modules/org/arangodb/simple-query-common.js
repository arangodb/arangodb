module.define("org/arangodb/simple-query-common", function(exports, module) {
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

var arangodb = require("org/arangodb");

var ArangoError = arangodb.ArangoError;

// forward declaration
var SimpleQueryArray;
var SimpleQueryNear;
var SimpleQueryWithin;

// -----------------------------------------------------------------------------
// --SECTION--                                              GENERAL ARRAY CURSOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralArrayCursor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief array query
////////////////////////////////////////////////////////////////////////////////

function GeneralArrayCursor (documents, skip, limit) {
  this._documents = documents;
  this._countTotal = documents.length;
  this._skip = skip;
  this._limit = limit;

  this.execute();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an array query
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.execute = function () {
  if (this._skip === null) {
    this._skip = 0;
  }

  var len = this._documents.length;
  var s = 0;
  var e = len;

  // skip from the beginning
  if (0 < this._skip) {
    s = this._skip;

    if (e < s) {
      s = e;
    }
  }

  // skip from the end
  else if (this._skip < 0) {
    var skip = -this._skip;

    if (skip < e) {
      s = e - skip;
    }
  }

  // apply limit
  if (this._limit !== null) {
    if (s + this._limit < e) {
      e = s + this._limit;
    }
  }

  this._current = s;
  this._stop = e;

  this._countQuery = e - s;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype._PRINT = function (context) {
  var text;

  text = "GeneralArrayCursor([.. " + this._documents.length + " docs ..])";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all elements of the cursor
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.toArray = 
GeneralArrayCursor.prototype.elements = function () {
  return this._documents;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the count of the cursor
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.count = function () {
  return this._countTotal;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.hasNext = function () {
  return this._current < this._stop;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.next = function() {
  if (this._current < this._stop) {
    return this._documents[this._current++];
  }

  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the result
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.dispose = function() {
  this._documents = null;
  this._skip = null;
  this._limit = null;
  this._countTotal = null;
  this._countQuery = null;
  this._current = null;
  this._stop = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      SIMPLE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief simple query
////////////////////////////////////////////////////////////////////////////////

function SimpleQuery () {
  this._execution = null;
  this._skip = 0;
  this._limit = null;
  this._countQuery = null;
  this._countTotal = null;
  this._batchSize = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief join limits
////////////////////////////////////////////////////////////////////////////////

function joinLimits (query, limit) {
  var q;

  // original limit is 0, keep it
  if (query._limit === 0) {
    query = query.clone();
  }

  // new limit is 0, use it
  else if (limit === 0) {
    query = query.clone();
    query._limit = 0;
  }

  // no old limit, use new limit
  else if (query._limit === null) {
    query = query.clone();
    query._limit = limit;
  }

  // use the smaller one
  else {
    query = query.clone();

    if (limit < query._limit) {
      query._limit = limit;
    }
  }

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.clone = function () {
  throw "cannot clone abstract query";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
///
/// @FUN{@FA{query}.execute(@FA{batchSize})}
///
/// Executes a simple query. If the optional @FA{batchSize} value is specified,
/// the server will return at most @FN{batchSize} values in one roundtrip.
/// The @FA{batchSize} cannot be adjusted after the query is first executed.
///
/// Note that there is no need to explicitly call the execute method if another
/// means of fetching the query results is chosen. The following two approaches
/// lead to the same result:
/// @code
/// result = db.users.all().toArray();
/// q = db.users.all(); q.execute(); result = [ ]; while (q.hasNext()) { result.push(q.next()); }
/// @endcode
///
/// The following two alternatives both use a @FA{batchSize} and return the same
/// result:
/// @code
/// q = db.users.all(); q.setBatchSize(20); q.execute(); while (q.hasNext()) { print(q.next()); }
/// q = db.users.all(); q.execute(20); while (q.hasNext()) { print(q.next()); }
/// @endcode
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.execute = function () {
  throw "cannot execute abstract query";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief limit
///
/// @FUN{@FA{query}.limit(@FA{number})}
///
/// Limits a result to the first @FA{number} documents. Specifying a limit of
/// @LIT{0} returns no documents at all. If you do not need a limit, just do
/// not add the limit operator. The limit must be non-negative.
///
/// In general the input to @FN{limit} should be sorted. Otherwise it will be
/// unclear which documents are used in the result set.
///
/// @EXAMPLES
/// 
/// @verbinclude simple2
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.limit = function (limit) {
  if (this._execution !== null) {
    throw "query is already executing";
  }

  if (limit < 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_BAD_PARAMETER;
    err.errorMessage = "limit must be non-negative";
    throw err;
  }

  return joinLimits(this, limit);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief skip
///
/// @FUN{@FA{query}.skip(@FA{number})}
///
/// Skips the first @FA{number} documents. If @FA{number} is positive, then skip
/// the number of documents. If @FA{number} is negative, then the total amount N
/// of documents must be known and the results starts at position (N +
/// @FA{number}).
///
/// In general the input to @FN{limit} should be sorted. Otherwise it will be
/// unclear which documents are used in the result set.
///
/// @EXAMPLES
///
/// @verbinclude simple8
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.skip = function (skip) {
  var query;
  var documents;

  if (skip === undefined || skip === null) {
    skip = 0;
  }

  if (this._execution !== null) {
    throw "query is already executing";
  }

  // no limit set, use or add skip
  if (this._limit === null) {
    query = this.clone();

    if (this._skip === null || this._skip === 0) {
      query._skip = skip;
    }
    else {
      query._skip += skip;
    }
  }

  // limit already set
  else {
    documents = this.clone().toArray();

    query = new SimpleQueryArray(documents);
    query._skip = skip;
    query._countTotal = documents._countTotal;
  }

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into an array
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.toArray = function () {
  var cursor;
  var result;

  this.execute();

  result = [];

  while (this.hasNext()) {
    result.push(this.next());
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the batch size
///
/// @FUN{@FA{cursor}.getBatchSize()}
///
/// Returns the batch size for queries. If the returned value is undefined, the
/// server will determine a sensible batch size for any following requests.
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.getBatchSize = function () {
  return this._batchSize;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the batch size for any following requests
///
/// @FUN{@FA{cursor}.setBatchSize(@FA{number})}
///
/// Sets the batch size for queries. The batch size determines how many results
/// are at most transferred from the server to the client in one chunk.
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.setBatchSize = function (value) {
  if (value >= 1) {
    this._batchSize = value;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents
///
/// @FUN{@FA{cursor}.count()}
///
/// The @FN{count} operator counts the number of document in the result set and
/// returns that number. The @FN{count} operator ignores any limits and returns
/// the total number of documents found.
///
/// @note Not all simple queries support counting. In this case @LIT{null} is
/// returned.
///
/// @FUN{@FA{cursor}.count(@LIT{true})}
///
/// If the result set was limited by the @FN{limit} operator or documents were
/// skiped using the @FN{skip} operator, the @FN{count} operator with argument
/// @LIT{true} will use the number of elements in the final result set - after
/// applying @FN{limit} and @FN{skip}.
///
/// @note Not all simple queries support counting. In this case @LIT{null} is
/// returned.
///
/// @EXAMPLES
///
/// Ignore any limit:
///
/// @verbinclude simple9
///
/// Counting any limit or skip:
///
/// @verbinclude simple10
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.count = function (applyPagination) {
  this.execute();

  if (applyPagination === undefined || ! applyPagination) {
    return this._countTotal;
  }

  return this._countQuery;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
///
/// @FUN{@FA{cursor}.hasNext()}
///
/// The @FN{hasNext} operator returns @LIT{true}, then the cursor still has
/// documents.  In this case the next document can be accessed using the
/// @FN{next} operator, which will advance the cursor.
///
/// @EXAMPLES
///
/// @verbinclude simple7
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.hasNext = function () {
  this.execute();

  return this._execution.hasNext();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
///
/// @FUN{@FA{cursor}.next()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, then the underlying
/// cursor of the simple query still has documents.  In this case the
/// next document can be accessed using the @FN{next} operator, which
/// will advance the underlying cursor. If you use @FN{next} on an
/// exhausted cursor, then @LIT{undefined} is returned.
///
/// @EXAMPLES
///
/// @verbinclude simple5
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.next = function () {
  this.execute();

  return this._execution.next();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief disposes the result
///
/// @FUN{@FA{cursor}.dispose()}
///
/// If you are no longer interested in any further results, you should call
/// @FN{dispose} in order to free any resources associated with the cursor.
/// After calling @FN{dispose} you can no longer access the cursor.
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.dispose = function() {
  if (this._execution !== null) {
    this._execution.dispose();
  }

  this._execution = null;
  this._countQuery = null;
  this._countTotal = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY ALL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief all query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryAll (collection) {
  this._collection = collection;
}

SimpleQueryAll.prototype = new SimpleQuery();
SimpleQueryAll.prototype.constructor = SimpleQueryAll;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.clone = function () {
  var query;

  query = new SimpleQueryAll(this._collection);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryAll(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                SIMPLE QUERY ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief array query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray = function (documents) {
  this._documents = documents;
};

SimpleQueryArray.prototype = new SimpleQuery();
SimpleQueryArray.prototype.constructor = SimpleQueryArray;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.clone = function () {
  var query;

  query = new SimpleQueryArray(this._documents);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.execute = function () {
  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    this._execution = new GeneralArrayCursor(this._documents, this._skip, this._limit);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryArray(documents)";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY BY EXAMPLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief query-by-example
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExample (collection, example) {
  this._collection = collection;
  this._example = example;
}

SimpleQueryByExample.prototype = new SimpleQuery();
SimpleQueryByExample.prototype.constructor = SimpleQueryByExample;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.clone = function () {
  var query;

  query = new SimpleQueryByExample(this._collection, this._example);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryByExample(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       RANGE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief range query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryRange (collection, attribute, left, right, type) {
  this._collection = collection;
  this._attribute = attribute;
  this._left = left;
  this._right = right;
  this._type = type;
}

SimpleQueryRange.prototype = new SimpleQuery();
SimpleQueryRange.prototype.constructor = SimpleQueryRange;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a range query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.clone = function () {
  var query;

  query = new SimpleQueryRange(this._collection,
                               this._attribute,
                               this._left,
                               this._right,
                               this._type);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a range query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryRange(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY GEO
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryGeo (collection, index) {
  this._collection = collection;
  this._index = index;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a geo index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype._PRINT = function (context) {
  var text;

  text = "GeoIndex("
       + this._collection.name()
       + ", "
       + this._index
       + ")";

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a near query for an index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this._collection, lat, lon, this._index);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within query for an index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this._collection, lat, lon, radius, this._index);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE QUERY NEAR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear = function (collection, latitude, longitude, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._distance = null;

  if (iid === undefined) {
    idx = collection.getIndexes();
    
    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];
      
      if (index.type === "geo1" || index.type === "geo2") {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }
    
  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }
};

SimpleQueryNear.prototype = new SimpleQuery();
SimpleQueryNear.prototype.constructor = SimpleQueryNear;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.clone = function () {
  var query;

  query = new SimpleQueryNear(this._collection, this._latitude, this._longitude, this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryNear("
       + this._collection.name()
       + ", "
       + this._latitude
       + ", "
       + this._longitude
       + ", "
       + this._index
       + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the distance attribute
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.distance = function (attribute) {
  var clone;

  clone = this.clone();

  if (attribute) {
    clone._distance = attribute;
  }
  else {
    clone._distance = "distance";
  }

  return clone;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               SIMPLE QUERY WITHIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin = function (collection, latitude, longitude, radius, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._radius = radius;
  this._distance = null;

  if (iid === undefined) {
    idx = collection.getIndexes();
    
    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];
      
      if (index.type === "geo1" || index.type === "geo2") {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }
    
  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }
};

SimpleQueryWithin.prototype = new SimpleQuery();
SimpleQueryWithin.prototype.constructor = SimpleQueryWithin;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.clone = function () {
  var query;

  query = new SimpleQueryWithin(this._collection,
                                this._latitude,
                                this._longitude,
                                this._radius, 
                                this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryWithin("
       + this._collection.name()
       + ", "
       + this._latitude
       + ", "
       + this._longitude
       + ", "
       + this._radius
       + ", "
       + this._index
       + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the distance attribute
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.distance = function (attribute) {
  var clone;

  clone = this.clone();

  if (attribute) {
    clone._distance = attribute;
  }
  else {
    clone._distance = "distance";
  }

  return clone;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             SIMPLE QUERY FULLTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryFulltext (collection, attribute, query, iid) {
  this._collection = collection;
  this._attribute = attribute;
  this._query = query;
  this._index = (iid === undefined ? null : iid);
 
  if (iid === undefined) {
    var idx = collection.getIndexes();
    var i;

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === "fulltext" && index.fields[0] === attribute) {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.indexSubstrings && ! this._index.indexSubstrings) {
          // prefer indexes that have substrings indexed
          this._index = index.id;
        }
      }
    }
  }
    
  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_FULLTEXT_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING.message;
    throw err;
  }
}

SimpleQueryFulltext.prototype = new SimpleQuery();
SimpleQueryFulltext.prototype.constructor = SimpleQueryFulltext;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a fulltext query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.clone = function () {
  var query;

  query = new SimpleQueryFulltext(this._collection, this._attribute, this._query, this._index);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a fulltext query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryFulltext("
       + this._collection.name()
       + ", "
       + this._attribute
       + ", \""
       + this._query
       + "\")";
  
  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
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
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryFulltext = SimpleQueryFulltext;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});
