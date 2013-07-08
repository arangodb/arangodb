/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, TRANSACTION */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDatabase
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                    ArangoDatabase
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

exports.ArangoDatabase = internal.ArangoDatabase;

var ArangoDatabase = exports.ArangoDatabase;

// must called after export
var ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;
var ArangoError = require("org/arangodb").ArangoError;
var ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;

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
/// @brief prints a database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._PRINT = function(context) {
  context.output += this.toString();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief strng representation of a database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype.toString = function(seen, path, names, level) {
  return "[ArangoDatabase \"" + this._path() + "\"]";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   query functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
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

ArangoDatabase.prototype._query = function (query, bindVars) {  
  var payload = {
    query: query,
    bindVars: bindVars || undefined 
  };
  return new ArangoStatement(this, payload).execute();
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      transactions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a transaction
///
/// @FUN{db._executeTransaction(@FA{object})}
///
/// Executes a server-side transaction, as specified by @FA{object}.
///
/// @FA{object} must have the following attributes:
/// - @LIT{collections}: a sub-object that defines which collections will be 
///   used in the transaction. @LIT{collections} can have these attributes:
///   - @LIT{read}: a single collection or a list of collections that will be
///     used in the transaction in read-only mode
///   - @LIT{write}: a single collection or a list of collections that will be
///     used in the transaction in write or read mode. 
/// - @LIT{action}: a Javascript function or a string with Javascript code
///   containing all the instructions to be executed inside the transaction.
///   If the code runs through successfully, the transaction will be committed
///   at the end. If the code throws an exception, the transaction will be 
///   rolled back and all database operations will be rolled back.
///
/// Additionally, @FA{object} can have the following optional attributes:
/// - @LIT{waitForSync}: boolean flag indicating whether the transaction
///   is forced to be synchronous.
/// - @LIT{lockTimeout}: a numeric value that can be used to set a timeout for 
///   waiting on collection locks. If not specified, a default value will be 
///   used. Setting @LIT{lockTimeout} to @LIT{0} will make ArangoDB not time 
///   out waiting for a lock.
/// - @LIT{params}: optional arguments passed to the function specified in 
///   @LIT{action}.
///
/// @EXAMPLES
///
/// @verbinclude shell_transaction
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._executeTransaction = function (data) {  
  return TRANSACTION(data);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              collection functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// @brief index id regex
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.indexRegex = /^([a-zA-Z0-9\-_]+)\/([0-9]+)$/;

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

  var pa = ArangoDatabase.indexRegex.exec(id);
  var err;

  if (pa === null) {
    err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code;
    err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message;
    throw err;
  }

  var col = this._collection(pa[1]);

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

  var pa = ArangoDatabase.indexRegex.exec(id);
  var err;

  if (pa === null) {
    err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code;
    err.errorMessage = internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message;
    throw err;
  }

  var col = this._collection(pa[1]);

  if (col === null) {
    err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message;
    throw err;
  }

  return col.dropIndex(id);
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
