/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
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

(function () {
  var internal = require("internal");
  var console = require("console");

  var ArangoCollection = internal.ArangoCollection;
  var ArangoError = internal.ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
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
/// @brief attachment collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.TYPE_ATTACHMENT = 4;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
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
      var re = /^([0-9]+)\/([0-9]+)/;
      var pa = re.exec(id);

      if (pa === null) {
        id = this._id + "/" + id;
      }
    }
    else if (id.hasOwnProperty("id")) {
      id = id.id;
    }

    for (i = 0;  i < indexes.length;  ++i) {
      var index = indexes[i];

      if (index.id === id) {
        return index;
      }
    }

    return null;
  };

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
/// @brief string representation of a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.toString = function (seen, path, names, level) {
    return "[ArangoCollection " + this._id + "]";
  };

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
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
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
      case ArangoCollection.TYPE_EDGE: type = "edge"; break;
      case ArangoCollection.TYPE_ATTACHMENT: type = "attachment"; break;
    }

    internal.output("[ArangoCollection ",
                    this._id, 
                    ", \"", this.name(), "\" (type ", type, ", status ", status, ")]");
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
