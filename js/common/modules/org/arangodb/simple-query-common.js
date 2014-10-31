/*jshint strict: false */
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
/// @brief array query
////////////////////////////////////////////////////////////////////////////////

function GeneralArrayCursor (documents, skip, limit, data) {
  this._documents = documents;
  this._countTotal = documents.length;
  this._skip = skip;
  this._limit = limit;
  this._extra = { };
  
  var self = this;
  if (data !== null && data !== undefined && typeof data === 'object') {
    [ 'stats', 'warnings', 'profile' ].forEach(function(d) {
      if (data.hasOwnProperty(d)) {
        self._extra[d] = data[d];
      }
    });
  }

  this.execute();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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
/// @brief return an extra value of the cursor
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.getExtra = function (name) {
  if (name === undefined) {
    return this._extra || { };
  }

  if (this._extra === undefined || this._extra === null) {
    return { };
  }

  if (! this._extra.hasOwnProperty(name)) {
    return null;
  }

  return this._extra[name];
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
  this._extra = null;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      SIMPLE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief join limits
////////////////////////////////////////////////////////////////////////////////

function joinLimits (query, limit) {
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
/// @startDocuBlock queryExecute
/// `query.execute(batchSize)`
///
/// Executes a simple query. If the optional batchSize value is specified,
/// the server will return at most batchSize values in one roundtrip.
/// The batchSize cannot be adjusted after the query is first executed.
///
/// **Note**: There is no need to explicitly call the execute method if another
/// means of fetching the query results is chosen. The following two approaches
/// lead to the same result:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{executeQuery}
/// ~ db._create("users");
/// ~ db.users.save({ name: "Gerhard" });
/// ~ db.users.save({ name: "Helmut" });
/// ~ db.users.save({ name: "Angela" });
///   result = db.users.all().toArray();
///   q = db.users.all(); q.execute(); result = [ ]; while (q.hasNext()) { result.push(q.next()); }
/// ~ db._drop("users")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// The following two alternatives both use a batchSize and return the same
/// result:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{executeQueryBatchSize}
/// ~ db._create("users");
/// ~ db.users.save({ name: "Gerhard" });
/// ~ db.users.save({ name: "Helmut" });
/// ~ db.users.save({ name: "Angela" });
///   q = db.users.all(); q.setBatchSize(20); q.execute(); while (q.hasNext()) { print(q.next()); }
///   q = db.users.all(); q.execute(20); while (q.hasNext()) { print(q.next()); }
/// ~ db._drop("users")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.execute = function () {
  throw "cannot execute abstract query";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief limit
/// @startDocuBlock queryLimit
/// `query.limit(number)`
///
/// Limits a result to the first *number* documents. Specifying a limit of
/// *0* returns no documents at all. If you do not need a limit, just do
/// not add the limit operator. The limit must be non-negative.
///
/// In general the input to *limit* should be sorted. Otherwise it will be
/// unclear which documents are used in the result set.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{queryLimit}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().toArray();
///   db.five.all().limit(2).toArray();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
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
/// @startDocuBlock querySkip
/// `query.skip(number)`
///
/// Skips the first *number* documents. If *number* is positive, then skip
/// the number of documents. If *number* is negative, then the total amount N
/// of documents must be known and the results starts at position (N +
/// *number*).
///
/// In general the input to *limit* should be sorted. Otherwise it will be
/// unclear which documents are used in the result set.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{querySkip}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().toArray();
///   db.five.all().skip(3).toArray();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
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
/// @startDocuBlock cursorGetBatchSize
/// `cursor.getBatchSize()`
///
/// Returns the batch size for queries. If the returned value is undefined, the
/// server will determine a sensible batch size for any following requests.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.getBatchSize = function () {
  return this._batchSize;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the batch size for any following requests
/// @startDocuBlock cursorSetBatchSize
/// `cursor.setBatchSize(number)`
///
/// Sets the batch size for queries. The batch size determines how many results
/// are at most transferred from the server to the client in one chunk.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.setBatchSize = function (value) {
  if (value >= 1) {
    this._batchSize = value;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents
/// @startDocuBlock cursorCount
/// `cursor.count()`
///
/// The *count* operator counts the number of document in the result set and
/// returns that number. The *count* operator ignores any limits and returns
/// the total number of documents found.
///
/// **Note**: Not all simple queries support counting. In this case *null* is
/// returned.
///
/// `cursor.count(true)`
///
/// If the result set was limited by the *limit* operator or documents were
/// skiped using the *skip* operator, the *count* operator with argument
/// *true* will use the number of elements in the final result set - after
/// applying *limit* and *skip*.
///
/// **Note**: Not all simple queries support counting. In this case *null* is
/// returned.
///
/// @EXAMPLES
///
/// Ignore any limit:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorCount}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().limit(2).count();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Counting any limit or skip:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorCountLimit}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().limit(2).count(true);
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
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
/// @startDocuBlock cursorHasNext
/// `cursor.hasNext()`
///
/// The *hasNext* operator returns *true*, then the cursor still has
/// documents. In this case the next document can be accessed using the
/// *next* operator, which will advance the cursor.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorHasNext}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   var a = db.five.all();
///   while (a.hasNext()) print(a.next());
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.hasNext = function () {
  this.execute();

  return this._execution.hasNext();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
/// @startDocuBlock cursorNext
/// `cursor.next()`
///
/// If the *hasNext* operator returns *true*, then the underlying
/// cursor of the simple query still has documents.  In this case the
/// next document can be accessed using the *next* operator, which
/// will advance the underlying cursor. If you use *next* on an
/// exhausted cursor, then *undefined* is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorNext}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().next();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.next = function () {
  this.execute();

  return this._execution.next();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief disposes the result
/// @startDocuBlock cursorDispose
/// `cursor.dispose()`
///
/// If you are no longer interested in any further results, you should call
/// *dispose* in order to free any resources associated with the cursor.
/// After calling *dispose* you can no longer access the cursor.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.dispose = function() {
  if (this._execution !== null) {
    this._execution.dispose();
  }

  this._execution = null;
  this._countQuery = null;
  this._countTotal = null;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY ALL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief all query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryAll (collection) {
  this._collection = collection;
}

SimpleQueryAll.prototype = new SimpleQuery();
SimpleQueryAll.prototype.constructor = SimpleQueryAll;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                SIMPLE QUERY ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief array query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray = function (documents) {
  this._documents = documents;
};

SimpleQueryArray.prototype = new SimpleQuery();
SimpleQueryArray.prototype.constructor = SimpleQueryArray;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY BY EXAMPLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief query-by-example
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExample (collection, example) {
  this._collection = collection;
  this._example = example;
}

SimpleQueryByExample.prototype = new SimpleQuery();
SimpleQueryByExample.prototype.constructor = SimpleQueryByExample;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.clone = function () {
  var query;

  query = new SimpleQueryByExample(this._collection, this._example);
  query._skip = this._skip;
  query._limit = this._limit;
  query._type = this._type;
  query._index = this._index;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                QUERY BY CONDITION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief query-by-condition
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryByCondition (collection, condition) {
  this._collection = collection;
  this._condition = condition;
}

SimpleQueryByCondition.prototype = new SimpleQuery();
SimpleQueryByCondition.prototype.constructor = SimpleQueryByCondition;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query-by-condition
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype.clone = function () {
  var query;

  query = new SimpleQueryByCondition(this._collection, this._condition);
  query._skip = this._skip;
  query._limit = this._limit;
  query._type = this._type;
  query._index = this._index;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print a query-by-condition
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryByCondition(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       RANGE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY GEO
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryGeo (collection, index) {
  this._collection = collection;
  this._index = index;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE QUERY NEAR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                               SIMPLE QUERY WITHIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                             SIMPLE QUERY FULLTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryByCondition = SimpleQueryByCondition;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryFulltext = SimpleQueryFulltext;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
