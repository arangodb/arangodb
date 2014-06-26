module.define("org/arangodb/arango-collection-common", function(exports, module) {
/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true */
/*global require */

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

var ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;

var arangodb = require("org/arangodb");

var ArangoError = arangodb.ArangoError;
var sprintf = arangodb.sprintf;
var db = arangodb.db;

var simple = require("org/arangodb/simple-query");

var SimpleQueryAll = simple.SimpleQueryAll;
var SimpleQueryByExample = simple.SimpleQueryByExample;
var SimpleQueryByCondition = simple.SimpleQueryByCondition;
var SimpleQueryRange = simple.SimpleQueryRange;
var SimpleQueryGeo = simple.SimpleQueryGeo;
var SimpleQueryNear = simple.SimpleQueryNear;
var SimpleQueryWithin = simple.SimpleQueryWithin;
var SimpleQueryFulltext = simple.SimpleQueryFulltext;

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                         constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is corrupted
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_CORRUPTED = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is new born
/// @deprecated
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_NEW_BORN = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is unloaded
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_UNLOADED = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is loaded
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_LOADED = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is unloading
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_UNLOADING = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is deleted
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_DELETED = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is currently loading
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_LOADING = 6;

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection
////////////////////////////////////////////////////////////////////////////////
  
ArangoCollection.TYPE_DOCUMENT = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.TYPE_EDGE = 3;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._PRINT = function (context) {
  var status = "unknown";
  var type = "unknown";
  var name = this.name();

  switch (this.status()) {
    case ArangoCollection.STATUS_NEW_BORN: status = "new born"; break;
    case ArangoCollection.STATUS_UNLOADED: status = "unloaded"; break;
    case ArangoCollection.STATUS_UNLOADING: status = "unloading"; break;
    case ArangoCollection.STATUS_LOADED: status = "loaded"; break;
    case ArangoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
    case ArangoCollection.STATUS_DELETED: status = "deleted"; break;
  }

  switch (this.type()) {
    case ArangoCollection.TYPE_DOCUMENT: type = "document"; break;
    case ArangoCollection.TYPE_EDGE:     type = "edge"; break;
  }

  var colors = require("internal").COLORS;
  var useColor = context.useColor;

  context.output += "[ArangoCollection ";
  if (useColor) { context.output += colors.COLOR_NUMBER; }
  context.output += this._id;
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += ", \"";
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += name || "unknown";
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += "\" (type " + type + ", status " + status + ")]";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into a string
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toString = function () {  
  return "[ArangoCollection: " + this._id + "]";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an all query for a collection
/// @startDocuBlock collectionAll
/// `collection.all()`
///
/// Selects all documents of a collection and returns a cursor. You can use
/// *toArray*, *next*, or *hasNext* to access the result. The result
/// can be limited using the *skip* and *limit* operator.
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionAll}
///   db.five.all();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Use *next* to loop over all documents:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionAllNext}
///   var a = db.five.all().toArray();
///   while (a.hasNext()) print(a.next());
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.all = function () {
  return new SimpleQueryAll(this);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example for a collection
/// @startDocuBlock collectionByExample
/// `collection.byExample(example)`
///
/// Selects all documents of a collection that match the specified
/// example and returns a cursor. 
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute. If you use 
/// 
/// *{ a : { c : 1 } }*
///
/// as example, then you will find all documents, such that the attribute
/// *a* contains a document of the form *{c : 1 }*. For example the document
///
/// *{ a : { c : 1 }\, b : 1 }*
///
/// will match, but the document 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will not.
///
/// However, if you use 
///
/// *{ a.c : 1 }*, 
///
/// then you will find all documents, which contain a sub-document in *a*
/// that has an attribute *c* of value *1*. Both the following documents 
///
/// *{ a : { c : 1 }\, b : 1 }* and 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will match.
///
/// `collection.byExample(path1, value1, ...)`
///
/// As alternative you can supply a list of paths and values.
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionByExample}
/// ~ db._create("users");
///   db.users.all().toArray();
///   db.users.byExample({ "id" : 323 }).toArray();
///   db.users.byExample({ "name" : "Peter" }).toArray();
///   db.users.byExample({ "name" : "Peter", "id" : 535 }).toArray();
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Use *next* to loop over all documents:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionByExampleNext}
/// ~ db._create("users");
///   var a = db.users.byExample( {"name" : "Peter" } );
///   while (a.hasNext()) print(a.next());
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExample = function (example) {
  var e;
  var i;

  // example is given as only argument
  if (arguments.length === 1) {
    e = example;
  }

  // example is given as list
  else {
    e = {};

    // create a REAL array, otherwise JSON.stringify will fail
    for (i = 0;  i < arguments.length;  i += 2) {
      e[arguments[i]] = arguments[i + 1];
    }
  }

  return new SimpleQueryByExample(this, e);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example using a hash index
/// @startDocuBLock collectionByExampleHash
/// `collection.byExampleHash(index, example)`
///
/// Selects all documents from the specified hash index that match the 
/// specified example example and returns a cursor. 
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute. If you use 
/// 
/// *{ a : { c : 1 } }*
///
/// as example, then you will find all documents, such that the attribute
/// *a* contains a document of the form *{c : 1 }*. For example the document
///
/// *{ a : { c : 1 }\, b : 1 }*
///
/// will match, but the document 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will not.
///
/// However, if you use 
///
/// *{ a.c : 1 }*, 
///
/// then you will find all documents, which contain a sub-document in *a*
/// that has an attribute @LIT{c} of value *1*. Both the following documents 
///
/// *{ a : { c : 1 }\, b : 1 }* and 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will match.
///
/// `collection.byExampleHash(index-id, path1, value1, ...)`
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExampleHash = function (index, example) {
  var sq = this.byExample(example);
  sq._index = index;
  sq._type = "hash";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example using a skiplist index
/// @startDocuBlock collectionByExampleSkiplist
/// `collection}.byExampleSkiplist(index, example)
///
/// Selects all documents from the specified skiplist index that match the 
/// specified example example and returns a cursor. 
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute. If you use 
/// 
/// *{ a : { c : 1 } }*
///
/// as example, then you will find all documents, such that the attribute
/// *a* contains a document of the form *{c : 1 }*. For example the document
///
/// *{ a : { c : 1 }\, b : 1 }*
///
/// will match, but the document 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will not.
///
/// However, if you use 
///
/// *{ a.c : 1 }*, 
///
/// then you will find all documents, which contain a sub-document in *a*
/// that has an attribute @LIT{c} of value *1*. Both the following documents 
///
/// *{ a : { c : 1 }\, b : 1 }*and 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will match.
///
/// `collection.byExampleHash(index-id, path1, value1, ...)`
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExampleSkiplist = function (index, example) {
  var sq = this.byExample(example);
  sq._index = index;
  sq._type = "skiplist";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example using a bitarray index
/// @startDocuBlock collectionByExampleBitArray
/// `collection.byExampleBitarray(index, example)`
///
/// Selects all documents from the specified bitarray index that match the 
/// specified example example and returns a cursor. 
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute. If you use 
/// 
/// *{ a : { c : 1 } }*
///
/// as example, then you will find all documents, such that the attribute
/// *a* contains a document of the form *{c : 1 }*. For example the document
///
/// *{ a : { c : 1 }\, b : 1 }*
///
/// will match, but the document 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will not.
///
/// However, if you use 
///
/// *{ a.c : 1 }*, 
///
/// then you will find all documents, which contain a sub-document in *a*
/// that has an attribute @LIT{c} of value *1*. Both the following documents 
///
/// *{ a : { c : 1 }\, b : 1 }*and 
///
/// *{ a : { c : 1\, b : 1 } }*
///
/// will match.
///
/// `collection.byExampleHash(index-id, path1, value1, ...)`
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExampleBitarray = function (index, example) {
  var sq = this.byExample(example);
  sq._index = index;
  sq._type = "bitarray";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-condition using a skiplist index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byConditionSkiplist = function (index, condition) {
  var sq = new SimpleQueryByCondition(this, condition);
  sq._index = index;
  sq._type = "skiplist";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-condition using a bitarray index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byConditionBitarray = function (index, condition) {
  var sq = new SimpleQueryByCondition(this, condition);
  sq._index = index;
  sq._type = "bitarray";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a range query for a collection
/// @startDocuBlock collectionRange
/// `collection.range(attribute, left, right)`
///
/// Selects all documents of a collection such that the *attribute* is
/// greater or equal than *left* and strictly less than *right*.
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute.
///
/// For range queries it is required that a skiplist index is present for the
/// queried attribute. If no skiplist index is present on the attribute, an
/// error will be thrown.
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionRange}
/// ~ db._create("example");
///   l = db.example.range("age", 10, 13).toArray();
/// ~ db._drop("example")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.range = function (name, left, right) {
  return new SimpleQueryRange(this, name, left, right, 0);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a closed range query for a collection
/// @startDocuBlock
/// `collection.closedRange(attribute, left, right)`
///
/// Selects all documents of a collection such that the *attribute* is
/// greater or equal than *left* and less or equal than *right*.
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute.
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @TINYEXAMPLE{simple-query-closed-range-to-array,convert into a list}
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.closedRange = function (name, left, right) {
  return new SimpleQueryRange(this, name, left, right, 1);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a geo index selection
/// @startDocuBlock collectionGeo
/// `collection.geo(location-attribute)`
///
/// Looks up a geo index defined on attribute *location-attribute*.
///
/// Returns a geo index object if an index was found. The *near* or 
/// *within* operators can then be used to execute a geo-spatial query on 
/// this particular index.
///
/// This is useful for collections with multiple defined geo indexes.
///
/// `collection.geo(location-attribute, true)`
///
/// Looks up a geo index on a compound attribute *location-attribute*.
///
/// Returns a geo index object if an index was found. The *near* or 
/// *within* operators can then be used to execute a geo-spatial query on 
/// this particular index.
///
/// `collection.geo(latitude-attribute, longitude-attribute)`
///
/// Looks up a geo index defined on the two attributes *latitude-attribute*
/// and *longitude-attribute*.
///
/// Returns a geo index object if an index was found. The *near* or 
/// *within* operators can then be used to execute a geo-spatial query on 
/// this particular index.
///
/// @EXAMPLES
///
/// Assume you have a location stored as list in the attribute *home*
/// and a destination stored in the attribute *work*. Then you can use the
/// *geo* operator to select which geo-spatial attributes (and thus which
/// index) to use in a near query.
///
/// @TINYEXAMPLE{simple-query-geo,use a specific index}
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.geo = function(loc, order) {
  var idx;

  var locateGeoIndex1 = function(collection, loc, order) {
    var inds = collection.getIndexes();
    var i;
    
    for (i = 0;  i < inds.length;  ++i) {
      var index = inds[i];
      
      if (index.type === "geo1") {
        if (index.fields[0] === loc && index.geoJson === order) {
          return index;
        }
      }
    }
    
    return null;
  };

  var locateGeoIndex2 = function(collection, lat, lon) {
    var inds = collection.getIndexes();
    var i;
    
    for (i = 0;  i < inds.length;  ++i) {
      var index = inds[i];
      
      if (index.type === "geo2") {
        if (index.fields[0] === lat && index.fields[1] === lon) {
          return index;
        }
      }
    }
    
    return null;
  };

  if (order === undefined) {
    if (typeof loc === "object") {
      idx = this.index(loc);
    }
    else {
      idx = locateGeoIndex1(this, loc, false);
    }
  }
  else if (typeof order === "boolean") {
    idx = locateGeoIndex1(this, loc, order);
  }
  else {
    idx = locateGeoIndex2(this, loc, order);
  }

  if (idx === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.code;
    err.errorMessage = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }

  return new SimpleQueryGeo(this, idx.id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a near query for a collection
/// @startDocuBlock collectionNear
/// `collection.near(latitude, longitude)`
///
/// The returned list is sorted according to the distance, with the nearest 
/// document to the coordinate (*latitude*, *longitude*) coming first. 
/// If there are near documents of equal distance, documents are chosen randomly 
/// from this set until the limit is reached. It is possible to change the limit 
/// using the *limit* operator.
///
/// In order to use the *near* operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the *geo* operator to select a particular index.
///
/// **Note**: *near* does not support negative skips. However, you can still use
/// *limit* followed to *skip*.
///
/// `collection.near(latitude, longitude).limit(limit)`
///
/// Limits the result to limit documents instead of the default 100.
///
/// **Note**: Unlike with multiple explicit limits, limit} will raise
/// the implicit default limit imposed by *within*.
///
/// `collection.near(latitude, longitude).distance()`
///
/// This will add an attribute *distance* to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// `collection.near(latitude, longitude).distance(name)`
///
/// This will add an attribute *name* to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @EXAMPLES
///
/// To get the nearst two locations:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionNear}
///   db.geo.near(0,0).limit(2).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// If you need the distance as well, then you can use the *distance*
/// operator:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionNearDistance}
///   db.geo.near(0,0).distance().limit(2).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this, lat, lon);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within query for a collection
/// @startDocuBlock collectionWithin
/// `collection.within(latitude, longitude, radius)`
///
/// This will find all documents within a given radius around the coordinate
/// (*latitude*, *longitude*). The returned list is sorted by distance,
/// beginning with the nearest document. 
///
/// In order to use the *within* operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the *geo* operator to select a particular index.
///
/// `collection.within(latitude, longitude, radius).distance()`
///
/// This will add an attribute *_distance* to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// `collection.within(latitude, longitude, radius).distance(name)`
///
/// This will add an attribute *name* to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @EXAMPLES
///
/// To find all documents within a radius of 2000 km use:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionWithin}
///   db.geo.within(0, 0, 2000 * 1000).distance().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this, lat, lon, radius);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a fulltext query for a collection
/// @startDocuBlock collectionFulltext
/// `collection.fulltext(attribute, query)`
///
/// This will find the documents from the collection's fulltext index that match the search
/// query.
///
/// In order to use the *fulltext* operator, a fulltext index must be defined for the
/// collection, for the specified attribute. If multiple fulltext indexes are defined
/// for the collection and attribute, the most capable one will be selected.
///
/// @EXAMPLES
///
/// To find all documents which contain the terms *text* and *word*:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionFulltext}
///   db.emails.fulltext("text", "word").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.fulltext = function (attribute, query, iid) {
  return new SimpleQueryFulltext(this, attribute, query, iid);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief iterators over some elements of a collection
/// @startDocuBlock collectionIterate
/// `collection.iterate(iterator, options)`
///
/// Iterates over some elements of the collection and apply the function
/// *iterator* to the elements. The function will be called with the
/// document as first argument and the current number (starting with 0)
/// as second argument.
///
/// *options* must be an object with the following attributes:
///
/// - *limit* (optional, default none): use at most *limit* documents.
///
/// - *probability* (optional, default all): a number between *0* and
///   *1*. Documents are chosen with this probability.
///
/// @EXAMPLES
///
/// @code
/// arango> db.example.getIndexes().map(function(x) { return x.id; });
/// ["93013/0"]
/// arango> db.example.index("93013/0");
/// { "id" : "93013/0", "type" : "primary", "fields" : ["_id"] }
/// @endcode
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.iterate = function (iterator, options) {
  var probability = 1.0;
  var limit = null;
  var stmt;
  var cursor;
  var pos;

  // TODO: this is not optimal for the client, there should be an HTTP call handling
  // everything on the server

  if (options !== undefined) {
    if (options.hasOwnProperty("probability")) {
      probability = options.probability;
    }

    if (options.hasOwnProperty("limit")) {
      limit = options.limit;
    }
  }

  if (limit === null) {
    if (probability >= 1.0) {
      cursor = this.all();
    }
    else {
      stmt = sprintf("FOR d IN %s FILTER rand() >= @prob RETURN d", this.name());
      stmt = db._createStatement({ query: stmt });

      if (probability < 1.0) {
        stmt.bind("prob", probability);
      }

      cursor = stmt.execute();
    }
  }
  else {
    if (typeof limit !== "number") {
      var error = new ArangoError();
      error.errorNum = arangodb.errors.ERROR_ILLEGAL_NUMBER.code;
      error.errorMessage = "expecting a number, got " + String(limit);

      throw error;
    }

    if (probability >= 1.0) {
      cursor = this.all().limit(limit);
    }
    else {
      stmt = sprintf("FOR d IN %s FILTER rand() >= @prob LIMIT %d RETURN d",
                     this.name(), limit);
      stmt = db._createStatement({ query: stmt });

      if (probability < 1.0) {
        stmt.bind("prob", probability);
      }

      cursor = stmt.execute();
    }
  }

  pos = 0;

  while (cursor.hasNext()) {
    var document = cursor.next();

    iterator(document, pos);

    pos++;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  document methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents matching an example
/// @startDocuBlock documentsCollectionRemoveByExample
/// `collection.removeByExample(example)`
///
/// Removes all documents matching an example.
///
/// `collection.removeByExample(document, waitForSync)`
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document deletion operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `collection.removeByExample(document, waitForSync, limit)`
///
/// The optional *limit* parameter can be used to restrict the number of 
/// removals to the specified value. If *limit* is specified but less than the
/// number of documents in the collection, it is undefined which documents are
/// removed.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveByExample}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
///   db.example.removeByExample( {Hello : "world"} );
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example, waitForSync, limit) {
  throw "cannot call abstract removeByExample function";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces documents matching an example
/// @startDocuBlock documentsCollectionReplaceByExample
/// `collection.replaceByExample(example, newValue)`
///
/// Replaces all documents matching an example with a new document body.
/// The entire document body of each document matching the *example* will be 
/// replaced with *newValue*. The document meta-attributes such as *_id*,
/// *_key*, *_from*, *_to* will not be replaced.
///
/// `collection.replaceByExample(document, newValue, waitForSync)`
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document replacement operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `collection.replaceByExample(document, newValue, waitForSync, limit)`
///
/// The optional *limit* parameter can be used to restrict the number of 
/// replacements to the specified value. If *limit* is specified but less than 
/// the number of documents in the collection, it is undefined which documents are
/// replaced.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionReplaceByExample}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
///   db.example.replaceByExample({ Hello: "world" }, {Hello: "mars"}, false, 5);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example, newValue, waitForSync, limit) {
  throw "cannot call abstract replaceByExample function";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief partially updates documents matching an example
/// @startDocuBlock documentsCollectionUpdateByExample
/// `collection.updateByExample(example, newValue)`
///
/// Partially updates all documents matching an example with a new document body.
/// Specific attributes in the document body of each document matching the 
/// *example* will be updated with the values from *newValue*. 
/// The document meta-attributes such as *_id*, *_key*, *_from*, 
/// *_to* cannot be updated.
///
/// `collection.updateByExample(document, newValue, keepNull, waitForSync)`
///
/// The optional *keepNull* parameter can be used to modify the behavior when
/// handling *null* values. Normally, *null* values are stored in the
/// database. By setting the *keepNull* parameter to *false*, this behavior
/// can be changed so that all attributes in *data* with *null* values will 
/// be removed from the target document.
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document replacement operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `collection.updateByExample(document, newValue, keepNull, waitForSync, limit)`
///
/// The optional *limit* parameter can be used to restrict the number of 
/// updates to the specified value. If *limit* is specified but less than 
/// the number of documents in the collection, it is undefined which documents are
/// updated.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdateByExample}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
///   db.example.updateByExample({ Hello: "world" }, { Hello: "foo", Hello: "bar" }, false);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example, newValue, keepNull, waitForSync, limit) {
  throw "cannot call abstract updateExample function";
};

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
