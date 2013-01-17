/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
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

var internal = require("internal");
var simple = require("org/arangodb/simple-query");

var ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;
var ArangoError = require("org/arangodb/arango-error").ArrangoError;

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._PRINT = function () {
  var status = "unknown";
  var type = "unknown";

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

  internal.output("[ArangoCollection ", 
                  internal.colors.COLOR_NUMBER, this._id, internal.colors.COLOR_RESET,
                  ", \"", 
                  internal.colors.COLOR_STRING, this.name(), internal.colors.COLOR_RESET, 
                  "\" (type ", type, ", status ", status, ")]");
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
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an all query for a collection
///
/// @FUN{all()}
///
/// Selects all documents of a collection and returns a cursor. You can use
/// @FN{toArray}, @FN{next}, or @FN{hasNext} to access the result. The result
/// can be limited using the @FN{skip} and @FN{limit} operator.
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

ArangoCollection.prototype.all = function () {
  return new simple.SimpleQueryAll(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example for a collection
///
/// @FUN{@FA{collection}.byExample(@FA{example})}
///
/// Selects all documents of a collection that match the specified
/// example and returns a cursor. 
///
/// You can use @FN{toArray}, @FN{next}, or @FN{hasNext} to access the
/// result. The result can be limited using the @FN{skip} and @FN{limit}
/// operator.
///
/// An attribute name of the form @LIT{a.b} is interpreted as attribute path,
/// not as attribute. If you use 
/// 
/// @LIT{{ a : { c : 1 } }} 
///
/// as example, then you will find all documents, such that the attribute
/// @LIT{a} contains a document of the form @LIT{{c : 1 }}. E.g., the document
///
/// @LIT{{ a : { c : 1 }\, b : 1 }} 
///
/// will match, but the document 
///
/// @LIT{{ a : { c : 1\, b : 1 } }}
///
/// will not.
///
/// However, if you use 
///
/// @LIT{{ a.c : 1 }}, 
///
/// then you will find all documents, which contain a sub-document in @LIT{a}
/// that has an attribute @LIT{c} of value @LIT{1}. E.g., both documents 
///
/// @LIT{{ a : { c : 1 }\, b : 1 }} and 
///
/// @LIT{{ a : { c : 1\, b : 1 } }}
///
/// will match.
///
/// @FUN{@FA{collection}.byExample(@FA{path1}, @FA{value1}, ...)}
///
/// As alternative you can supply a list of paths and values.
///
/// @EXAMPLES
///
/// Use @FN{toArray} to get all documents at once:
///
/// @TINYEXAMPLE{simple18,convert into a list}
///
/// Use @FN{next} to loop over all documents:
///
/// @TINYEXAMPLE{simple19,iterate over the result-set}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExample = function () {
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

  // create a REAL array, otherwise JSON.stringify will fail
  return new simple.SimpleQueryByExample(this, example);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a range query for a collection
///
/// @FUN{@FA{collection}.range(@FA{attribute}, @FA{left}, @FA{right})}
///
/// Selects all documents of a collection such that the @FA{attribute} is
/// greater or equal than @FA{left} and strictly less than @FA{right}.
///
/// You can use @FN{toArray}, @FN{next}, or @FN{hasNext} to access the
/// result. The result can be limited using the @FN{skip} and @FN{limit}
/// operator.
///
/// An attribute name of the form @LIT{a.b} is interpreted as attribute path,
/// not as attribute.
///
/// For range queries it is required that a skiplist index is present for the
/// queried attribute. If no skiplist index is present on the attribute, an
/// error will be thrown.
///
/// @EXAMPLES
///
/// Use @FN{toArray} to get all documents at once:
///
/// @TINYEXAMPLE{simple-query-range-to-array,convert into a list}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.range = function (name, left, right) {
  return new simple.SimpleQueryRange(this, name, left, right, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a closed range query for a collection
///
/// @FUN{@FA{collection}.closedRange(@FA{attribute}, @FA{left}, @FA{right})}
///
/// Selects all documents of a collection such that the @FA{attribute} is
/// greater or equal than @FA{left} and less or equal than @FA{right}.
///
/// You can use @FN{toArray}, @FN{next}, or @FN{hasNext} to access the
/// result. The result can be limited using the @FN{skip} and @FN{limit}
/// operator.
///
/// An attribute name of the form @LIT{a.b} is interpreted as attribute path,
/// not as attribute.
///
/// @EXAMPLES
///
/// Use @FN{toArray} to get all documents at once:
///
/// @TINYEXAMPLE{simple-query-closed-range-to-array,convert into a list}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.closedRange = function (name, left, right) {
  return new simple.SimpleQueryRange(this, name, left, right, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a geo index selection
///
/// @FUN{@FA{collection}.geo(@FA{location})}
////////////////////////////////////////////
///
/// The next @FN{near} or @FN{within} operator will use the specific geo-spatial
/// index.
///
/// @FUN{@FA{collection}.geo(@FA{location}, @LIT{true})}
////////////////////////////////////////////////////////
///
/// The next @FN{near} or @FN{within} operator will use the specific geo-spatial
/// index.
///
/// @FUN{@FA{collection}.geo(@FA{latitude}, @FA{longitude})}
////////////////////////////////////////////////////////////
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
/// @TINYEXAMPLE{simple-query-geo,use a specific index}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.geo = function(loc, order) {
  var idx;

  var locateGeoIndex1 = function(collection, loc, order) {
    var inds = collection.getIndexes();
    
    for (var i = 0;  i < inds.length;  ++i) {
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
    
    for (var i = 0;  i < inds.length;  ++i) {
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
    err.errorNum = internal.errors.ERROR_QUERY_GEO_INDEX_MISSING.code;
    err.errorMessage = internal.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }

  return new simple.SimpleQueryGeo(this, idx.id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a near query for a collection
///
/// @FUN{@FA{collection}.near(@FA{latitude}, @FA{longitude})}
/////////////////////////////////////////////////////////////
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
/// @note @FN{near} does not support negative skips. However, you can still use
/// @FN{limit} followed to @FN{skip}.
///
/// @FUN{@FA{collection}.near(@FA{latitude}, @FA{longitude}).limit(@FA{limit})}
///////////////////////////////////////////////////////////////////////////////
///
/// Limits the result to @FA{limit} documents instead of the default 100.
///
/// @note Unlike with multiple explicit limits, @FA{limit} will raise
/// the implicit default limit imposed by @FN{within}.
///
/// @FUN{@FA{collection}.near(@FA{latitude}, @FA{longitude}).distance()}
////////////////////////////////////////////////////////////////////////
///
/// This will add an attribute @LIT{distance} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @FUN{@FA{collection}.near(@FA{latitude}, @FA{longitude}).distance(@FA{name})}
/////////////////////////////////////////////////////////////////////////////////
///
/// This will add an attribute @FA{name} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @EXAMPLES
///
/// To get the nearst two locations:
///
/// @TINYEXAMPLE{simple-query-near,nearest two location}
///
/// If you need the distance as well, then you can use the @FN{distance}
/// operator:
///
/// @TINYEXAMPLE{simple-query-near2,nearest two location with distance in meter}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.near = function (lat, lon) {
  return new simple.SimpleQueryNear(this, lat, lon);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within query for a collection
///
/// @FUN{@FA{collection}.within(@FA{latitude}, @FA{longitude}, @FA{radius})}
////////////////////////////////////////////////////////////////////////////
///
/// This will find all documents with in a given radius around the coordinate
/// (@FA{latitude}, @FA{longitude}). The returned list is sorted by distance.
///
/// In order to use the @FN{within} operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the @FN{geo} operator to select a particular index.
///
/// @FUN{@FA{collection}.within(@FA{latitude}, @FA{longitude}, @FA{radius})@LATEXBREAK.distance()}
//////////////////////////////////////////////////////////////////////////////////////////////////
///
/// This will add an attribute @LIT{_distance} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @FUN{@FA{collection}.within(@FA{latitude}, @FA{longitude}, @FA{radius})@LATEXBREAK.distance(@FA{name})}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// This will add an attribute @FA{name} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @EXAMPLES
///
/// To find all documents within a radius of 2000 km use:
///
/// @TINYEXAMPLE{simple-query-within,within a radius}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.within = function (lat, lon, radius) {
  return new simple.SimpleQueryWithin(this, lat, lon, radius);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a fulltext query for a collection
///
/// @FUN{@FA{collection}.fulltext(@FA{attribute}, @FA{query})}
////////////////////////////////////////////////////////////////////////////
///
/// This will find the documents from the collection's fulltext index that match the search
/// query.
///
/// In order to use the @FN{fulltext} operator, a fulltext index must be defined for the
/// collection, for the specified attribute. If multiple fulltext indexes are defined
/// for the collection and attribute, the most capable one will be selected.
///
/// @EXAMPLES
///
/// To find all documents which contain the terms @LIT{foo} and @LIT{bar}:
///
/// @TINYEXAMPLE{simple-query-fulltext,complete match query}
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.fulltext = function (attribute, query, iid) {
  return new simple.SimpleQueryFulltext(this, attribute, query, iid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterators over some elements of a collection
///
/// @FUN{@FA{collection}.iterate(@FA{iterator}, @FA{options})}
///
/// Iterates over some elements of the collection and apply the function
/// @FA{iterator} to the elements. The function will be called with the
/// document as first argument and the current number (starting with 0)
/// as second argument.
///
/// @FA{options} must be an object with the following attributes:
///
/// - @LIT{limit} (optional, default none): use at most @LIT{limit} documents.
///
/// - @LIT{probability} (optional, default all): a number between @LIT{0} and
///   @LIT{1}. Documents are chosen with this probability.
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

// TODO this is not optiomal for the client, there should a HTTP call handling
//      everything on the server

ArangoCollection.prototype.iterate = function (iterator, options) {
  var probability = 1.0;
  var limit = null;
  var stmt;
  var cursor;
  var pos;

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
      stmt = internal.sprintf("FOR d IN %s FILTER rand() >= @prob RETURN d", this.name());
      stmt = internal.db._createStatement({ query: stmt });

      if (probability < 1.0) {
        stmt.bind("prob", probability);
      }

      cursor = stmt.execute();
    }
  }
  else {
    if (typeof limit !== "number") {
      var error = new ArangoError();
      error.errorNum = internal.errors.ERROR_ILLEGAL_NUMBER.code;
      error.errorMessage = "expecting a number, got " + String(limit);

      throw error;
    }

    if (probability >= 1.0) {
      cursor = this.all().limit(limit);
    }
    else {
      stmt = internal.sprintf("FOR d IN %s FILTER rand() >= @prob LIMIT %d RETURN d",
                              this.name(), limit);
      stmt = internal.db._createStatement({ query: stmt });

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
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents matching an example
///
/// @FUN{@FA{collection}.removeByExample(@FA{example})}
///
/// Removes all document matching an example.
///
/// @FUN{@FA{collection}.removeByExample(@FA{document}, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force synchronisation
/// of the document deletion operation to disk even in case that the
/// @LIT{waitForSync} flag had been disabled for the entire collection.  Thus,
/// the @FA{waitForSync} parameter can be used to force synchronisation of just
/// specific operations. To use this, set the @FA{waitForSync} parameter to
/// @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @EXAMPLES
///
/// @code
/// arangod> db.content.removeByExample({ "domain": "de.celler" })
/// @endcode
////////////////////////////////////////////////////////////////////////////////

// TODO this is not optiomal for the client, there should a HTTP call handling
//      everything on the server

ArangoCollection.prototype.removeByExample = function (example, waitForSync) {
  var documents;

  documents = this.byExample(example);

  while (documents.hasNext()) {
    var document = documents.next();

    this.remove(document, true, waitForSync);
  }
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
