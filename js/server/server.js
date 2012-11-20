/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global require, db, edges, ModuleCache, Module,
  ArangoCollection, ArangoDatabase,
  ArangoError, ShapedJson,
  SYS_DEFINE_ACTION */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript server functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleInternal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief internal module
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");
  var console = require("console");

  internal.db = db;
  internal.edges = db;
  internal.ArangoCollection = ArangoCollection;

  if (typeof SYS_DEFINE_ACTION === "undefined") {
    internal.defineAction = function() {
      console.error("SYS_DEFINE_ACTION not available");
    };

    internal.actionLoaded = function() {
    }
  }
  else {
    internal.defineAction = SYS_DEFINE_ACTION;

    internal.actionLoaded = function() {
      var modules;
      var i;
      
      console.debug("actions loaded");
      
      modules = internal.db._collection("_modules");

      if (modules !== null) {
	modules = modules.byExample({ autoload: true }).toArray();

	for (i = 0;  i < modules.length;  ++i) {
	  var module = modules[i];

	  console.debug("autoloading module: %s", module.path);

	  try {
	    require(module.path);
	  }
	  catch (err) {
	    console.error("while loading '%s': %s", module.path, String(err));
	  }
	}
      }
    }
  }

  if (typeof SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION === "undefined") {
    internal.executeGlobalContextFunction = function() {
      console.error("SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION not available");
    };
  }
  else {
    internal.executeGlobalContextFunction = SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             Module "simple-query"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleSimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief simple-query module
////////////////////////////////////////////////////////////////////////////////

  try {
    require("simple-query");
  }
  catch (err) {
    console.error("while loading 'simple-query' module: %s", err);
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        ShapedJson
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json
////////////////////////////////////////////////////////////////////////////////

  ShapedJson.prototype._PRINT = function(seen, path, names, level) {
    if (this instanceof ShapedJson) {
      internal.printObject(this, seen, path, names, level);
    }
    else {
      internal.output(this.toString());
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       ArangoError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an error
////////////////////////////////////////////////////////////////////////////////

  ArangoError.prototype._PRINT = function() {
    var errorNum = this.errorNum;
    var errorMessage = this.errorMessage;

    internal.output("[ArangoError ", errorNum, ": ", errorMessage, "]");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief converts error to string
////////////////////////////////////////////////////////////////////////////////

  ArangoError.prototype.toString = function() {
    var errorNum = this.errorNum;
    var errorMessage = this.errorMessage;

    return "[ArangoError " + errorNum + ": " + errorMessage + "]";
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    ArangoDatabase
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create a new statement
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._createStatement = function (data) {  
    return new ArangoStatement(this, data);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create and execute a new statement
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._query = function (data) {  
    return new ArangoStatement(this, { query: data }).execute();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
///
/// @FUN{db._drop(@FA{collection})}
///
/// Drops a @FA{collection} and all its indexes.
///
/// @FUN{db._drop(@FA{collection-identifier})}
///
/// Drops a collection identified by @FA{collection-identifier} and all its
/// indexes. No error is thrown if there is no such collection.
///
/// @FUN{db._drop(@FA{collection-name})}
///
/// Drops a collection named @FA{collection-name} and all its indexes. No error
/// is thrown if there is no such collection.
///
/// @EXAMPLES
///
/// Drops a collection:
///
/// @verbinclude shell_collection-drop-db
///
/// Drops a collection identified by name:
///
/// @verbinclude shell_collection-drop-name-db
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._drop = function(name) {
    var collection = name;

    if (! (name instanceof ArangoCollection)) {
      collection = internal.db._collection(name);
    }

    if (collection === null) {
      return;
    }

    return collection.drop();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
///
/// @FUN{db._truncate(@FA{collection})}
///
/// Truncates a @FA{collection}, removing all documents but keeping all its
/// indexes.
///
/// @FUN{db._truncate(@FA{collection-identifier})}
///
/// Truncates a collection identified by @FA{collection-identified}. No error is
/// thrown if there is no such collection.
///
/// @FUN{db._truncate(@FA{collection-name})}
///
/// Truncates a collection named @FA{collection-name}. No error is thrown if
/// there is no such collection.
///
/// @EXAMPLES
///
/// Truncates a collection:
///
/// @verbinclude shell_collection-truncate-db
///
/// Truncates a collection identified by name:
///
/// @verbinclude shell_collection-truncate-name-db
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._truncate = function(name) {
    var collection = name;

    if (! (name instanceof ArangoCollection)) {
      collection = internal.db._collection(name);
    }

    if (collection === null) {
      return;
    }

    collection.truncate();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an index
///
/// @FUN{db._index(@FA{index-handle})}
///
/// Returns the index with @FA{index-handle} or null if no such index exists.
///
/// @EXAMPLES
///
/// @verbinclude shell_index-read-db
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._index = function(id) {
    if (id.hasOwnProperty("id")) {
      id = id.id;
    }

    var re = /^([0-9]+)\/([0-9]+)/;
    var pa = re.exec(id);
    var err;

    if (pa === null) {
      err = new ArangoError();
      err.errorNum = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code;
      err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message;
      throw err;
    }

    var col = this._collection(parseInt(pa[1]));

    if (col === null) {
      err = new ArangoError();
      err.errorNum = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
      err.errorMessage = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message;
      throw err;
    }

    var indexes = col.getIndexes();
    var i;

    for (i = 0;  i < indexes.length;  ++i) {
      var index = indexes[i];

      if (index.id === id) {
        return index;
      }
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
///
/// @FUN{db._dropIndex(@FA{index})}
///
/// Drops the @FA{index}.  If the index does not exists, then @LIT{false} is
/// returned. If the index existed and was dropped, then @LIT{true} is
/// returned. Note that you cannot drop the primary index.
///
/// @FUN{db._dropIndex(@FA{index-handle})}
///
/// Drops the index with @FA{index-handle}.
///
/// @EXAMPLES
///
/// @verbinclude shell_index-drop-index-db
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._dropIndex = function(id) {
    if (id.hasOwnProperty("id")) {
      id = id.id;
    }

    var re = /^([0-9]+)\/([0-9]+)/;
    var pa = re.exec(id);
    var err;

    if (pa === null) {
      err = new ArangoError();
      err.errorNum = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code;
      err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message;
      throw err;
    }

    var col = this._collection(parseInt(pa[1]));

    if (col === null) {
      err = new ArangoError();
      err.errorNum = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
      err.errorMessage = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message;
      throw err;
    }

    return col.dropIndex(id);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a database
////////////////////////////////////////////////////////////////////////////////

    ArangoDatabase.prototype._PRINT = function(seen, path, names, level) {
      internal.output("[ArangoDatabase \"" + this._path + "\"]");
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief strng representation of a database
////////////////////////////////////////////////////////////////////////////////

    ArangoDatabase.prototype.toString = function(seen, path, names, level) {
      return "[ArangoDatabase \"" + this._path + "\"]";
    };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
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
/// @brief converts collection into an array
///
/// @FUN{@FA{collection}.toArray()}
///
/// Converts the collection into an array of documents. Never use this call
/// in a production environment.
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.toArray = function() {
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
/// @verbinclude shell_collection-truncate
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.truncate = function() {
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
/// @verbinclude shell_index-read
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.index = function(id) {
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
/// @brief prints a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype._PRINT = function() {
    var status = type = "unknown";

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
/// @brief strng representation of a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.toString = function(seen, path, names, level) {
    return "[ArangoCollection " + this._id + "]";
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoStatement
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////
  
  try {
    var SQ = require("simple-query-basics");
    var SB = require("statement-basics");
    internal.ArangoStatement = SB.ArangoStatement;
    ArangoStatement = SB.ArangoStatement;
  }
  catch (err) {
    console.error("while loading 'statement-basics' module: %s", err);
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return the results
////////////////////////////////////////////////////////////////////////////////

  SB.ArangoStatement.prototype.parse = function () {
    var result = AHUACATL_PARSE(this._query); 

    return { "bindVars" : result.parameters, "collections" : result.collections };
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief explain a query and return the results
////////////////////////////////////////////////////////////////////////////////

  SB.ArangoStatement.prototype.explain = function () {
    return AHUACATL_EXPLAIN(this._query); 
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// This will return a cursor with the query results in case of success.
////////////////////////////////////////////////////////////////////////////////

  SB.ArangoStatement.prototype.execute = function () {
    var result = AHUACATL_RUN(this._query, 
                              this._bindVars, 
                              this._doCount != undefined ? this._doCount : false, 
                              null, 
                              true);  
    return new SQ.GeneralArrayCursor(result, 0, null);
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
