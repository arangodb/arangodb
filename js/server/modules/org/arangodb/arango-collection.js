/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// @brief converts collection into an array
///
/// @FUN{@FA{collection}.toArray()}
///
/// Converts the collection into an array of documents. Never use this call
/// in a production environment.
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toArray = function () {
  return this.ALL(null, null).documents;
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
/// arango> col.save({ "Hallo" : "World" });
/// { "_id" : "91022/1532814", "_rev" : 1532814 }
/// arango> col.count();
/// 1
/// arango> col.truncate();
/// arango> col.count();
/// 0
/// @endcode
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
  return internal.db._truncate(this);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   index functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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

  var documents = simple.byExample(this, e, 0, 1);

  if (0 < documents.documents.length) {
    return documents.documents[0];
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example, 
                                                       waitForSync, 
                                                       limit) {
  var deleted = 0;
  var documents;

  if (limit === 0) {
    return 0;
  }

  documents = this.byExample(example);
  if (limit > 0) {
    documents = documents.limit(limit);
  }

  while (documents.hasNext()) {
    var document = documents.next();

    if (this.remove(document, true, waitForSync)) {
      deleted++;
    }
  }

  return deleted;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example, 
                                                        newValue, 
                                                        waitForSync, 
                                                        limit) {
  var replaced = 0;
  var documents;
  
  if (limit === 0) {
    return 0;
  }

  if (typeof newValue !== "object") {
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid value for parameter 'newValue'";

    throw err;
  }

  documents = this.byExample(example);
  if (limit > 0) {
    documents = documents.limit(limit);
  }

  while (documents.hasNext()) {
    var document = documents.next();

    if (this.replace(document, newValue, true, waitForSync)) {
      replaced++;
    }
  }

  return replaced;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief partially updates documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example, 
                                                       newValue, 
                                                       keepNull, 
                                                       waitForSync, 
                                                       limit) {
  var updated = 0;
  var documents;
  
  if (limit === 0) {
    return 0;
  }
  
  if (typeof newValue !== "object") {
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid value for parameter 'newValue'";

    throw err;
  }

  documents = this.byExample(example);
  if (limit > 0) {
    documents = documents.limit(limit);
  }

  while (documents.hasNext()) {
    var document = documents.next();

    if (this.update(document, newValue, true, keepNull, waitForSync)) {
      updated++;
    }
  }

  return updated;
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
