////////////////////////////////////////////////////////////////////////////////
/// @brief Avocado Simple Query Language
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
var AvocadoCollection = internal.AvocadoCollection;
var AvocadoEdgesCollection = internal.AvocadoEdgesCollection;

// -----------------------------------------------------------------------------
// --SECTION--                                                AVOCADO COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an all query for a collection
///
/// @FUN{all()}
///
/// Selects all documents of a collection. You can use @FN{toArray}, @FN{next},
/// @FN{nextRef}, or @FN{hasNext} to access the result. The result can be
/// limited using the @FN{skip} and @FN{limit} operator.
///
/// @EXAMPLES
///
/// Use @FN{toArray} to get all documents at once:
///
/// @verbinclude simple3
///
/// Use @FN{next} to loop over all documents:
///
/// @verbinclude simple4
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.all = function () {
  return new SimpleQueryAll(this);
}

AvocadoEdgesCollection.prototype.all = AvocadoCollection.prototype.all;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a near query for a collection
///
/// @FUN{near(@FA{latitude}, @FA{longitude})}
///
/// The default will find at most 100 documents near the coordinate
/// (@FA{latitude}, @FA{longitude}). The returned list is sorted according to
/// the distance, with the nearest document coming first. If there are near
/// documents of equal distance, documents are chosen randomly from this set
/// until the limit is reached. It is possible to change the limit using the
/// @FA{limit} operator.
///
/// In order to use the @FN{near} operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the @FN{geo} operator to select a particular index.
///
/// @FUN{near(@FA{latitude}, @FA{longitude}).limit(@FA{limit})}
///
/// Limits the result to @FA{limit} documents. Note that @FA{limit} can be more
/// than 100, this will raise the default limit.
///
/// @FUN{near(@FA{latitude}, @FA{longitude}).distance()}
///
/// This will add an attribute @LIT{distance} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @FUN{near(@FA{latitude}, @FA{longitude}).distance(@FA{name})}
///
/// This will add an attribute @FA{name} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @EXAMPLES
///
/// To get the nearst two locations:
///
/// @verbinclude simple14
///
/// If you need the distance as well, then you can use:
///
/// @verbinclude simple15
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this, lat, lon);
}

AvocadoEdgesCollection.prototype.near = AvocadoCollection.prototype.near;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within query for a collection
///
/// @FUN{within(@FA{latitude}, @FA{longitude}, @FA{radius})}
///
/// This will find all documents with in a given radius around the coordinate
/// (@FA{latitude}, @FA{longitude}). The returned list is sorted by distance.
///
/// In order to use the @FN{within} operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the @FN{geo} operator to select a particular index.
///
/// @FUN{within(@FA{latitude}, @FA{longitude}, @FA{radius}).distance()}
///
/// This will add an attribute @LIT{_distance} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @FUN{within(@FA{latitude}, @FA{longitude}, @FA{radius}).distance(@FA{name})}
///
/// This will add an attribute @FA{name} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @EXAMPLES
///
/// To find all documents within a radius of 2000 km use:
///
/// @verbinclude simple17
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this, lat, lon, radius);
}

AvocadoEdgesCollection.prototype.within = AvocadoCollection.prototype.within;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a geo index selection
///
/// @FUN{geo(@FA{location})}
///
/// The next @FN{near} or @FN{within} operator will use the specific geo-spatial
/// index.
///
/// @FUN{geo(@FA{location}, @LIT{true})}
///
/// The next @FN{near} or @FN{within} operator will use the specific geo-spatial
/// index.
///
/// @FUN{geo(@FA{latitude}, @FA{longitude})}
///
/// The next @FN{near} or @FN{within} operator will use the specific geo-spatial
/// index.
///
/// @EXAMPLES
///
/// Assume you have a location stored as list in the attribute @LIT{home}
/// and a destination stored in the attribute @LIT{work}. Than you can use the
/// @FN{geo} operator to select, which coordinates to use in a near query.
///
/// @verbinclude simple16
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.geo = function(loc, order) {
  var idx;

  var locateGeoIndex1 = function(collection, loc, order) {
    var inds = collection.getIndexes();
    
    for (var i = 0;  i < inds.length;  ++i) {
      var index = inds[i];
      
      if (index.type == "geo") {
        if (index.location == loc && index.geoJson == order) {
          return index;
        }
      }
    }
    
    return null;
  };

  var locateGeoIndex2 = function(collection, lat, lon) {
    var inds = collection.getIndexes();
    
    for (var i = 0;  i < inds.length;  ++i) {
      var index = inds[i];
      
      if (index.type == "geo") {
        if (index.latitude == lat && index.longitude == lon) {
          return index;
        }
      }
    }
    
    return null;
  };

  if (order === undefined) {
    idx = locateGeoIndex1(this, loc, false);
  }
  else if (typeof order === "boolean") {
    idx = locateGeoIndex1(this, loc, order);
  }
  else {
    idx = locateGeoIndex2(this, loc, order);
  }

  if (idx == null) {
    throw "cannot find a suitable geo index";
  }

  return new SimpleQueryGeo(this, idx.iid);
}

AvocadoEdgesCollection.prototype.geo = AvocadoCollection.geo;

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
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief join limits
////////////////////////////////////////////////////////////////////////////////

function JoinLimits (query, limit) {
  var q;

  // original limit is 0, keep it
  if (query._limit == 0) {
    query = query.clone();
  }

  // new limit is 0, use it
  else if (limit == 0) {
    query = query.clone();
    query._limit = 0;
  }

  // no old limit, use new limit
  else if (query._limit == null) {
    query = query.clone();
    query._limit = limit
  }

  // both are positive, use the smaller one
  else if (0 < query._limit && 0 < limit) {
    query = query.clone();

    if (limit < query._limit) {
      query._limit = limit;
    }
  }

  // both are negative, use the greater one
  else if (query._limit < 0 && limit < 0) {
    query = query.clone();

    if (query._limit < limit) {
      query._limit = limit;
    }
  }

  // different sign
  else {
    q = query.toArray();

    q = new SimpleQueryArray(q);
    q._limit = limit;
    q._countTotal = query._countTotal;

    query = q;
  }

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.clone = function () {
  throw "cannot clone abstract query";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.execute = function () {
  throw "cannot execute abstract query";
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
/// @brief limit
///
/// @FUN{limit(@FA{number})}
///
/// Limits a result to the first @FA{number} documents. Specifying a limit of
/// @CODE{0} returns no documents at all. If you do not need a limit, just do
/// not add the limit operator. If you specifiy a negtive limit of @CODE{-n},
/// this will return the last @CODE{n} documents instead.
///
/// In general the input to @FN{limit} should be sorted. Otherwise it will be
/// unclear which documents are used in the result set.
///
/// @EXAMPLES
/// 
/// @verbinclude simple2
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.limit = function (limit) {
  if (this._execution != null) {
    throw "query is already executing";
  }

  return JoinLimits(this, limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skip
///
/// @FUN{skip(@FA{number})}
///
/// Skips the first @FA{number} documents.
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

  if (skip == null) {
    skip = 0;
  }

  if (skip < 0) {
    throw "skip must be non-negative";
  }

  if (this._execution != null) {
    throw "query is already executing";
  }

  // no limit set, use or add skip
  if (this._limit == null) {
    query = this.clone();

    if (this._skip == null || this._skip == 0) {
      query._skip = skip;
    }
    else {
      query._skip += skip;
    }
  }

  // limit already skip
  else {
    documents = this.clone();

    query = new SimpleQueryArray(documents.toArray());
    query._skip = skip;
    query._countTotal = documents._countTotal;
  }

  return query;
}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents
///
/// @FUN{count()}
///
/// The @FN{count} operator counts the number of document in the result set and
/// returns that number. The @FN{count} operator ignores any limits and returns
/// the total number of documents found.
///
/// @FUN{count(@LIT{true})}
///
/// If the result set was limited by the @FN{limit} operator or documents were
/// skiped using the @FN{skip} operator, the @FN{count} operator with argument
/// @LIT{true} will use the number of elements in the final result set - after
/// applying @FN{limit} and @FN{skip}.
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
  else {
    return this._countQuery;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
///
/// @FUN{hasNext()}
///
/// The @FN{hasNext} operator returns @LIT{true}, then the cursor still has
/// documents.  In this case the next document can be accessed using the
/// @FN{next} operator, which will advance the cursor.
///
/// @verbinclude simple7
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.hasNext = function () {
  this.execute();

  return this._execution.hasNext();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
///
/// @FUN{next()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, then the underlying
/// cursor of the simple query still has documents.  In this case the
/// next document can be accessed using the @FN{next} operator, which
/// will advance the underlying cursor. If you use @FN{next} on an
/// exhausted cursor, then @LIT{undefined} is returned.
///
/// @verbinclude simple5
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.next = function() {
  this.execute();

  return this._execution.next();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document reference
///
/// @FUN{nextRef()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, then the underlying cursor
/// of the simple query still has documents.  In this case the next document
/// reference can be accessed using the @FN{nextRef} operator, which will
/// advance the cursor.  If you use @FN{next} on an exhausted cursor, then
/// @LIT{undefined} is returned.
///
/// @verbinclude simple6
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.nextRef = function() {
  this.execute();

  return this._execution.nextRef();
}

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
// --SECTION--                                                 private functions
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.execute = function () {
  var documents;

  if (this._execution == null) {
    if (this._skip == null || this._skip <= 0) {
      this._skip = 0;
    }

    documents = this._collection.ALL(this._skip, this._limit);

    this._execution = new SimpleQueryArray(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype._PRINT = function () {
  var text;

  text = "SimpleQueryAll(" + this._collection.name() + ")";

  if (this._skip != null && this._skip != 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit != null) {
    text += ".limit(" + this._limit + ")";
  }

  internal.output(text);
}

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
/// @brief select query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExample (collection, example) {
  this._collection = collection;
  this._example = example;
}

SimpleQueryByExample.prototype = new SimpleQuery();
SimpleQueryByExample.prototype.constructor = SimpleQueryByExample;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example for a collection
///
/// @FUN{byExample(@FA{example})}
///
/// Selects all documents of a collection that match the specified
/// @FA{example}. The example must be specified as an object, with the object
/// attributes being the search values. Allowed attribute types for searching
/// are numbers, strings, and boolean values.
///
/// You can use @FN{toArray}, @FN{next}, @FN{nextRef}, or @FN{hasNext} to access
/// the result. The result can be limited using the @FN{skip} and @FN{limit}
/// operator.
///
/// @EXAMPLES
///
/// Use @FN{toArray} to get all documents at once:
///
/// @verbinclude simple18
///
/// Use @FN{next} to loop over all documents:
///
/// @verbinclude simple19
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.byExample = function (example) {
  return new SimpleQueryByExample(this, example);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.execute = function () {
  var documents;

  if (this._execution == null) {
    if (this._skip == null || this._skip <= 0) {
      this._skip = 0;
    }

    var queryString = "SELECT c FROM `" + this._collection.name() + "` c";

    if (!(this._example instanceof Object)) {
      throw "invalid example specification";
    }
   
    var found = false; 
    for (var i in this._example) {
      if (!this._example.hasOwnProperty(i)) {
        continue;
      }

      var typeString = typeof(this._example[i]);
      if (typeString !== "number" && typeString !== "string" && typeString !== "boolean") {
        throw "invalid example specification for key " + i;
      }

      if (found) {
        queryString += "&& "; 
      }
      else {
        queryString += " WHERE ";
        found = true;
      }

      queryString += "c.`" + i + "` == ";
      if (typeString == "number") {
        queryString += this._example[i];
      }
      else if (typeString == "string") {
        queryString += QuoteJSONString(this._example[i]);
      }
      else if (typeString == "boolean") {
        queryString += this._example[i];
      }
    }

    var cursor = AQL_STATEMENT(queryString, undefined);  
    if (cursor instanceof AvocadoQueryError) {
      throw cursor.message;
    }

    var documents = { "count" : cursor.count(), "total" : cursor.count(), "documents": [] };
    while (cursor.hasNext()) {
      documents.documents.push(cursor.next());
    }
    cursor.dispose();

    this._execution = new SimpleQueryArray(documents.documents);
    this._execution._skip = this._skip;
    this._execution._limit = this._limit;
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype._PRINT = function () {
  var text;

  text = "SimpleQueryByExample(" + this._collection.name() + ")";

  if (this._skip != null && this._skip != 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit != null) {
    text += ".limit(" + this._limit + ")";
  }

  internal.output(text);
}

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

function SimpleQueryArray (documents) {
  this._documents = documents;
  this._current = 0;
  this._countTotal = documents.length;
}

SimpleQueryArray.prototype = new SimpleQuery();
SimpleQueryArray.prototype.constructors = SimpleQueryArray;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
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
  query._countTotal = this._countTotal;

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an array query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.execute = function () {
  if (this._execution == null) {
    this._execution = true;

    if (this._skip == null || this._skip <= 0) {
      this._skip = 0;
    }

    if (this._skip != 0 && this._limit == null) {
      this._current = this._skip;

      this._countQuery = this._documents.length - this._skip;

      if (this._countQuery < 0) {
        this._countQuery = 0;
      }
    }
    else if (this._limit != null) {
      var documents;
      var start;
      var end;

      if (0 == this._limit) {
        start = 0;
        end = 0;
      }
      else if (0 < this._limit) {
        start = this._skip;
        end = this._skip + this._limit;
      }
      else {
        start = this._documents.length + this._limit - this._skip;
        end = this._documents.length - this._skip;
      }

      if (start < 0) {
        start = 0;
      }

      if (this._documents.length < end) {
        end = this._documents.length;
      }

      documents = [];

      for (var i = start;  i < end;  ++i) {
        documents.push(this._documents[i]);
      }

      this._documents = documents;
      this._skip = 0;
      this._limit = null;
      this._countQuery = this._documents.length;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype._PRINT = function () {
  var text;

  text = "SimpleQueryArray([.. " + this._documents.length + " docs ..])";

  if (this._skip != null && this._skip != 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit != null) {
    text += ".limit(" + this._limit + ")";
  }

  internal.output(text);
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
/// @brief counts the number of documents
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.count = function (applyPagination) {
  this.execute();

  if (applyPagination === undefined || ! applyPagination) {
    return this._countTotal;
  }
  else {
    return this._countQuery;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.hasNext = function () {
  this.execute();

  return this._current < this._documents.length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.next = function() {
  this.execute();

  if (this._current < this._documents.length) {
    return this._documents[this._current++];
  }
  else {
    return undefined;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document reference
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.nextRef = function() {
  this.execute();

  if (this._current < this._documents.length) {
    return this._documents[this._current++]._id;
  }
  else {
    return undefined;
  }
}

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief print a geo index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype._PRINT = function () {
  var text;

  text = "GeoIndex("
       + this._collection.name()
       + ", "
       + this._index
       + ")";

  internal.output(text);
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
/// @brief constructs a near query for an index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this._collection, lat, lon, this._index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within query for an index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this._collection, lat, lon, radius, this._index);
}

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
/// @brief all query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryNear (collection, latitude, longitude, iid) {
  var idx;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._distance = null;

  if (iid == null) {
    idx = collection.getIndexes();
    
    for (var i = 0;  i < idx.length;  ++i) {
      var index = idx[i];
      
      if (index.type == "geo") {
        if (this._index == null) {
          this._index = index.iid;
        }
        else if (index.iid < this._index) {
          this._index = index.iid;
        }
      }
    }
  }
    
  if (this._index == null) {
    throw "a geo-index must be known";
  }
}

SimpleQueryNear.prototype = new SimpleQuery();
SimpleQueryNear.prototype.constructor = SimpleQueryNear;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.clone = function () {
  var query;

  query = new SimpleQueryNear(this._collection, this._latitude, this._longitude, this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.execute = function () {
  var result;
  var documents;
  var distances;
  var limit;

  if (this._execution == null) {
    if (this._skip == null || this._skip <= 0) {
      this._skip = 0;
    }

    if (this._limit == null) {
      limit = this._skip + 100;
    }
    else {
      limit = this._skip + this._limit;
    }

    result = this._collection.NEAR(this._index, this._latitude, this._longitude, limit);
    documents = result.documents;
    distances = result.distances;

    if (this._distance != null) {
      for (var i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new SimpleQueryArray(result.documents);
    this._execution._skip = this._skip;
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype._PRINT = function () {
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

  if (this._skip != null && this._skip != 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit != null) {
    text += ".limit(" + this._limit + ")";
  }

  internal.output(text);
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
/// @brief 
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
}

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
/// @brief all query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryWithin (collection, latitude, longitude, radius, iid) {
  var idx;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._radius = radius;
  this._distance = null;

  if (iid == null) {
    idx = collection.getIndexes();
    
    for (var i = 0;  i < idx.length;  ++i) {
      var index = idx[i];
      
      if (index.type == "geo") {
        if (this._index == null) {
          this._index = index.iid;
        }
        else if (index.iid < this._index) {
          this._index = index.iid;
        }
      }
    }
  }
    
  if (this._index == null) {
    throw "a geo-index must be known";
  }
}

SimpleQueryWithin.prototype = new SimpleQuery();
SimpleQueryWithin.prototype.constructor = SimpleQueryWithin;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.clone = function () {
  var query;

  query = new SimpleQueryWithin(this._collection, this._latitude, this._longitude, this._radius, this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.execute = function () {
  var result;
  var documents;
  var distances;
  var limit;

  if (this._execution == null) {
    result = this._collection.WITHIN(this._index, this._latitude, this._longitude, this._radius);
    documents = result.documents;
    distances = result.distances;

    if (this._distance != null) {
      for (var i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new SimpleQueryArray(result.documents);
    this._execution._skip = this._skip;
    this._execution._limit = this._limit;
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype._PRINT = function () {
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

  if (this._skip != null && this._skip != 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit != null) {
    text += ".limit(" + this._limit + ")";
  }

  internal.output(text);
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
/// @brief 
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
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.AvocadoCollection = AvocadoCollection;
exports.AvocadoEdgesCollection = AvocadoEdgesCollection;
exports.SimpleQuery = SimpleQuery;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryWithin = SimpleQueryWithin;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
