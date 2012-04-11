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

ModuleCache["/internal"].exports.db = db;
ModuleCache["/internal"].exports.edges = edges;
ModuleCache["/internal"].exports.AvocadoCollection = AvocadoCollection;
ModuleCache["/internal"].exports.AvocadoEdgesCollection = AvocadoEdgesCollection;

if (typeof SYS_DEFINE_ACTION === "undefined") {
  ModuleCache["/internal"].exports.defineAction = function() {
    console.error("SYS_DEFINE_ACTION not available");
  }
}
else {
  ModuleCache["/internal"].exports.defineAction = SYS_DEFINE_ACTION;
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
    PRINT_OBJECT(this, seen, path, names, level);
  }
  else {
    internal.output(this.toString());
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      AvocadoError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an error
////////////////////////////////////////////////////////////////////////////////

AvocadoError.prototype._PRINT = function() {
  var errorNum = this.errorNum;
  var errorMessage = this.errorMessage;

  internal.output("[AvocadoError ", errorNum, ": ", errorMessage, "]");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts error to string
////////////////////////////////////////////////////////////////////////////////

AvocadoError.prototype.toString = function() {
  var errorNum = this.errorNum;
  var errorMessage = this.errorMessage;

  return "[AvocadoError " + errorNum + ": " + errorMessage + "]";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   AvocadoDatabase
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
///
/// @FUN{db._drop(@FA{collection})}
///
/// Drops a @FA{collection} and all its indexes.
///
/// @FUN{db._drop(@FA{collection-name})}
///
/// Drops a collection named @FA{collection-name} and all its indexes.
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

AvocadoDatabase.prototype._drop = function(name) {
  var collection = name;

  if (typeof name === "string") {
    collection = db._collection(name);
  }

  if (collection == null) {
    return;
  }

  return collection.drop()
};

AvocadoEdges.prototype._drop = AvocadoDatabase._drop;

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
///
/// @FUN{db._truncate(@FA{collection})}
///
/// Truncates a @FA{collection}, removing all documents but keeping all its
/// indexes.
///
/// @FUN{db._truncate(@FA{collection-name})}
///
/// Truncates a collection named @FA{collection-name}.
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

AvocadoDatabase.prototype._truncate = function(name) {
  var collection = name;

  if (typeof name === "string") {
    collection = db._collection(name);
  }

  if (collection == null) {
    return;
  }

  var all = collection.ALL(null, null).documents;

  for (var i = 0;  i < all.length;  ++i) {
    collection.delete(all[i]._id);
  }
};

AvocadoEdges.prototype._truncate = AvocadoDatabase._truncate;

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a database
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._PRINT = function(seen, path, names, level) {
  internal.output("[AvocadoDatabase \"" + this._path + "\"]");
};

AvocadoEdges.prototype._PRINT =  function(seen, path, names, level) {
  internal.output("[AvocadoEdges \"" + this._path + "\"]");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief strng representation of a database
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype.toString = function(seen, path, names, level) {
  return "[AvocadoDatabase \"" + this._path + "\"]";
};

AvocadoEdges.prototype.toString = function(seen, path, names, level) {
  return "[AvocadoEdges \"" + this._path + "\"]";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AvocadoCollection
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is corrupted
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.STATUS_CORRUPTED = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is new born
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.STATUS_NEW_BORN = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is unloaded
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.STATUS_UNLOADED = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is loaded
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.STATUS_LOADED = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is unloading
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.STATUS_UNLOADING = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is deleted
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.STATUS_DELETED = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief converts collection into an array
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.toArray = function() {
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

AvocadoCollection.prototype.truncate = function() {
  return db._truncate(this);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an index of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.index = function(id) {
  var indexes = this.getIndexes();

  for (var i = 0;  i < indexes.length;  ++i) {
    var index = indexes[i];

    if (index.id == id) {
      return index;
    }
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype._PRINT = function() {
  var status = "unknown";

  switch (this.status()) {
    case AvocadoCollection.STATUS_NEW_BORN: status = "new born"; break;
    case AvocadoCollection.STATUS_UNLOADED: status = "unloaded"; break;
    case AvocadoCollection.STATUS_UNLOADING: status = "unloading"; break;
    case AvocadoCollection.STATUS_LOADED: status = "loaded"; break;
    case AvocadoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
    case AvocadoCollection.STATUS_DELETED: status = "deleted"; break;
  }
  
  SYS_OUTPUT("[AvocadoCollection ", this._id, ", \"", this.name(), "\" (status ", status, ")]");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief strng representation of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.toString = function(seen, path, names, level) {
  return "[AvocadoCollection " + this._id + "]";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
