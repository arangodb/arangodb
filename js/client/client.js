////////////////////////////////////////////////////////////////////////////////
/// @brief AvocadoShell client API
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
/// @author Achim Brandt
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default collection for saving queries
////////////////////////////////////////////////////////////////////////////////

var DEFAULT_QUERY_COLLECTION = "query";

////////////////////////////////////////////////////////////////////////////////
/// @brief help texts
////////////////////////////////////////////////////////////////////////////////

var HELP = "";
var helpQueries = "";
var helpAvocadoDatabase = "";
var helpAvocadoCollection = "";
var helpAvocadoQueryCursor = "";
var helpAvocadoStoredStatement = "";
var helpAvocadoStatement = "";
var helpExtended = "";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return a formatted type string for object
/// 
/// If the object has an id, it will be included in the string.
////////////////////////////////////////////////////////////////////////////////

function TRI_GetIdString (object, typeName) {
  var result = "[object " + typeName;
  
  if (object._id) {
    result += ":" + object._id;
  }
  else if (object.data && object.data._id) {
    result += ":" + object.data._id;
  }

  result += "]";

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles error results
/// 
/// throws an exception in case of an an error
////////////////////////////////////////////////////////////////////////////////

function TRI_CheckRequestResult (requestResult) {
  if (requestResult == undefined) {    
    requestResult = {
      "error" : true,
      "code"  : 0,
      "errorNum" : 0,
      "errorMessage" : "Unknown error. Request result is empty"
    }    
  }
  
  if (requestResult["error"] != undefined && requestResult["error"]) {    
    throw new AvocadoError(requestResult);
  }  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a formatted headline text 
////////////////////////////////////////////////////////////////////////////////

function TRI_CreateHelpHeadline (text) {
  var x = parseInt(Math.abs(78 - text.length) / 2);
  
  var p = "";
  for (var i = 0; i < x; ++i) {
    p += "-";
  }
  
  return "\n" + p + " " + text + " " + p + "\n";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief turn off pretty printing
////////////////////////////////////////////////////////////////////////////////

function print_plain (data) {
  var p = PRETTY_PRINT;
  PRETTY_PRINT = false;
  var c;
  if (typeof(COLOR_OUTPUT) != undefined) {
    c = COLOR_OUTPUT;
    COLOR_OUTPUT = undefined;
  }
  
  try {
    print(data);
    PRETTY_PRINT = p;
    if (typeof(c) != undefined) {
      COLOR_OUTPUT = c;
    }   
  }
  catch (e) {
    PRETTY_PRINT = p;
    if (typeof(c) != undefined) {
      COLOR_OUTPUT = c;
    }   
    throw e.message;    
  }  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing
////////////////////////////////////////////////////////////////////////////////

function start_pretty_print () {
  print("use pretty printing");
  PRETTY_PRINT=true;
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_pretty_print () {
  print("stop pretty printing");
  PRETTY_PRINT=false;
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing with optional color
////////////////////////////////////////////////////////////////////////////////

function start_color_print (color) {
  if (typeof(color) == "string") {
    COLOR_OUTPUT = color;
  }
  else {
    COLOR_OUTPUT = COLOR_BRIGHT;
  }
  print("start " + COLOR_OUTPUT + "color" + COLOR_OUTPUT_RESET + " printing");
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_color_print () {
  print("stop color printing");
  COLOR_OUTPUT = undefined;
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the overall help
////////////////////////////////////////////////////////////////////////////////

function help () {
  print(HELP);
  print(helpQueries);
  print(helpAvocadoDatabase);
  print(helpAvocadoCollection);
  print(helpAvocadoStatement);
  print(helpAvocadoQueryCursor);
  print(helpExtended);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief change internal.output to shell output
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"].exports.output = TRI_SYS_OUTPUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief log function
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"].exports.log = function(level, msg) {
  internal.output(level, ": ", msg, "\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the pager
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"].exports.start_pager = SYS_START_PAGER;

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the pager
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"].exports.stop_pager = SYS_STOP_PAGER;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      AvocadoError
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoError (error) {
  this.error = error.error;
  this.code = error.code;
  this.errorNum = error.errorNum;
  this.errorMessage = error.errorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an error
////////////////////////////////////////////////////////////////////////////////

AvocadoError.prototype._PRINT = function() {
  internal.output(this.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toString function
////////////////////////////////////////////////////////////////////////////////

AvocadoError.prototype.toString = function() {
  var result = "";
  if (typeof(COLOR_BRIGHT) != "undefined") {
    result = COLOR_BRIGHT + "Error: " + COLOR_OUTPUT_RESET;
  }
  else  {
    result = "Error: ";
  }

  result += "[" + this.code + ":" + this.errorNum + "] " + this.errorMessage;
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an error
////////////////////////////////////////////////////////////////////////////////

AvocadoError.prototype.toString = function() {
  var errorNum = this.errorNum;
  var errorMessage = this.errorMessage;

  return "[AvocadoError " + errorNum + ": " + errorMessage + "]";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   AvocadoDatabase
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoDatabase (connection) {
  this._connection = connection;
  this._collectionConstructor = AvocadoCollection;
}

ModuleCache["/internal"].exports.AvocadoDatabase = AvocadoDatabase;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for AvocadoDatabase
////////////////////////////////////////////////////////////////////////////////

helpAvocadoDatabase = TRI_CreateHelpHeadline("AvocadoDatabase help") +
'AvocadoDatabase constructor:                                        ' + "\n" +
' > db = new AvocadoDatabase(connection);                            ' + "\n" +
'                                                                    ' + "\n" +
'Administration Functions:                                           ' + "\n" +
'  _help();                       this help                          ' + "\n" +
'                                                                    ' + "\n" +
'Collection Functions:                                               ' + "\n" +
'  _collections()                 list all collections               ' + "\n" +
'  _collection(<identifier>)      get collection by identifier/name  ' + "\n" +
'  _create(<name>, <props>)       creates a new collection           ' + "\n" +
'  _truncate(<name>)              delete all documents               ' + "\n" +
'  _drop(<name>)                  delete a collection                ' + "\n" +
'                                                                    ' + "\n" +
'Document Functions:                                                 ' + "\n" +
'  _document(<id>)                 get document by handle            ' + "\n" +
'  _replace(<id>, <data>)          over-writes document              ' + "\n" +
'  _remove(<id>)                   deletes document                  ' + "\n" +
'                                                                    ' + "\n" +
'Query Functions:                                                    ' + "\n" +
'  _createStatement(<data>);      create and return select query     ' + "\n" +
'                                                                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  <collection names>             collection with the given name     ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoDatabase
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._help = function () {  
  print(helpAvocadoDatabase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the database object
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype.toString = function () {  
  return "[object AvocadoDatabase]";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              collection functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return all collections from the database
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._collections = function () {
  var requestResult = this._connection.GET("/_api/collection");
  
  TRI_CheckRequestResult(requestResult);

  if (requestResult["collections"] != undefined) {
    var collections = requestResult["collections"];
    var result = []
    
    // add all collentions to object
    for (var i = 0;  i < collections.length;  ++i) {
      var collection = new this._collectionConstructor(this, collections[i]);

      this[collection._name] = collection;
      result.push(collection);
    }
      
    return result;
  }
  
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single collection, identified by its id or name
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._collection = function (id) {
  var requestResult = this._connection.GET("/_api/collection/" + encodeURIComponent(id));
  
  // return null in case of not found
  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_AVOCADO_COLLECTION_NOT_FOUND.code) {
    return null;
  }

  // check all other errors and throw them
  TRI_CheckRequestResult(requestResult);

  var name = requestResult["name"];

  if (name != undefined) {
    return this[name] = new this._collectionConstructor(this, requestResult);
  }
  
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._create = function (name, properties) {
  var body = {
    "name" : name
  };

  if (properties != null) {
    if (properties.hasOwnProperty("waitForSync")) {
      body.waitForSync = properties.waitForSync;
    }

    if (properties.hasOwnProperty("journalSize")) {
      body.journalSize = properties.journalSize;
    }
  }

  var requestResult = this._connection.POST("/_api/collection", JSON.stringify(body));

  TRI_CheckRequestResult(requestResult);

  var name = requestResult["name"];

  if (name != undefined) {
    return this[name] = new this._collectionConstructor(this, requestResult);
  }
  
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._truncate = function (id) {
  for (var name in this) {
    if (this.hasOwnProperty(name)) {
      var collection = this[name];

      if (collection instanceof this._collectionConstructor) {
        if (collection._id == id || collection._name == id) {
          return collection.truncate();
        }
      }
    }
  }

  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._drop = function (id) {
  for (var name in this) {
    if (this.hasOwnProperty(name)) {
      var collection = this[name];

      if (collection instanceof this._collectionConstructor) {
        if (collection._id == id || collection._name == id) {
          return collection.drop();
        }
      }
    }
  }

  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns one index
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._index = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._connection.GET("/_api/index/" + id);

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes one index
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._dropIndex = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._connection.DELETE("/_api/index/" + id);

  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_AVOCADO_INDEX_NOT_FOUND.code) {
    return false;
  }

  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single document from the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._document = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev == null) {
    requestResult = this._connection.GET("/document/" + id);
  }
  else {
    requestResult = this._connection.GET("/document/" + id, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_AVOCADO_COLLECTION_NOT_FOUND.code) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code;
    }

    throw new AvocadoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._remove = function (id, overwrite) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var policy = "";

  if (overwrite) {
    policy = "?policy=last";
  }

  if (rev == null) {
    requestResult = this._connection.DELETE("/document/" + id + policy);
  }
  else {
    requestResult = this._connection.DELETE("/document/" + id + policy, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code;
    }

    if (overwrite) {
      if (requestResult.errorNum == internal.errors.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code) {
        return false;
      }
    }

    throw new AvocadoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._replace = function (id, data, overwrite) { 
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var policy = "";

  if (overwrite) {
    policy = "?policy=last";
  }

  if (rev == null) {
    requestResult = this._connection.PUT("/document/" + id + policy, JSON.stringify(data));
  }
  else {
    requestResult = this._connection.PUT("/document/" + id + policy, JSON.stringify(data), {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code;
    }

    throw new AvocadoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   query functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create a new statement
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._createStatement = function (data) {  
  return new AvocadoStatement(this, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      AvocadoEdges
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoEdges (connection) {
  this._connection = connection;
  this._collectionConstructor = AvocadoEdgesCollection;
}

AvocadoEdges.prototype = new AvocadoDatabase();

ModuleCache["/internal"].exports.AvocadoEdges = AvocadoEdges;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for AvocadoEdges
////////////////////////////////////////////////////////////////////////////////

helpAvocadoEdges = TRI_CreateHelpHeadline("AvocadoEdges help") +
'AvocadoEdges constructor:                                           ' + "\n" +
' > edges = new AvocadoEdges(connection);                            ' + "\n" +
'                                                                    ' + "\n" +
'Administration Functions:                                           ' + "\n" +
'  _help();                       this help                          ' + "\n" +
'                                                                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  <collection names>             collection with the given name     ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoEdges
////////////////////////////////////////////////////////////////////////////////

AvocadoEdges.prototype._help = function () {  
  print(helpAvocadoEdges);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the database object
////////////////////////////////////////////////////////////////////////////////

AvocadoEdges.prototype.toString = function () {  
  return "[object AvocadoEdges]";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AvocadoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoCollection (database, data) {
  this._database = database;

  if (typeof data === "string") {
    this._name = data;
  }
  else if (data != null) {
    this._id = data.id;
    this._name = data.name;
    this._status = data.status;
  }
}

ModuleCache["/internal"].exports.AvocadoCollection = AvocadoCollection;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for AvocadoCollection
////////////////////////////////////////////////////////////////////////////////

helpAvocadoCollection = TRI_CreateHelpHeadline("AvocadoCollection help") +
'AvocadoCollection constructor:                                      ' + "\n" +
' > col = db.mycoll;                                                 ' + "\n" +
' > col = db._create("mycoll");                                      ' + "\n" +
'                                                                    ' + "\n" +
'Administration Functions:                                           ' + "\n" +
'  name()                          collection name                   ' + "\n" +
'  status()                        status of the collection          ' + "\n" +
'  truncate()                      delete all documents              ' + "\n" +
'  properties()                    show collection properties        ' + "\n" +
'  drop()                          delete a collection               ' + "\n" +
'  load()                          load a collection into memeory    ' + "\n" +
'  unload()                        unload a collection from memory   ' + "\n" +
'  rename(new-name)                renames a collection              ' + "\n" +
'  refresh()                       refreshes the status and name     ' + "\n" +
'  _help();                        this help                         ' + "\n" +
'                                                                    ' + "\n" +
'Document Functions:                                                 ' + "\n" +
'  count()                         number of documents               ' + "\n" +
'  save(<data>)                    create document and return handle ' + "\n" +
'  document(<id>)                  get document by handle            ' + "\n" +
'  replace(<id>, <data>)           over-writes document              ' + "\n" +
'  delete(<id>)                    deletes document                  ' + "\n" +
'                                                                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                       database object                   ' + "\n" +
'  _id                             collection identifier             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.toString = function () {  
  return TRI_GetIdString(this, "AvocadoCollection");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype._PRINT = function () {  
  var status = "unknown";

  switch (this.status()) {
    case AvocadoCollection.STATUS_NEW_BORN: status = "new born"; break;
    case AvocadoCollection.STATUS_UNLOADED: status = "unloaded"; break;
    case AvocadoCollection.STATUS_UNLOADING: status = "unloading"; break;
    case AvocadoCollection.STATUS_LOADED: status = "loaded"; break;
    case AvocadoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
    case AvocadoCollection.STATUS_DELETED: status = "deleted"; break;
  }
  
  internal.output("[AvocadoCollection ", this._id, ", \"", this.name(), "\" (status " + status + ")]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoCollection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype._help = function () {  
  print(helpAvocadoCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.name = function () {
  if (this._name == null) {
    this.refresh();
  }

  return this._name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.status = function () {
  if (this._status == null) {
    this.refresh();
  }

  return this._status;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.properties = function (properties) {
  var requestResult;

  if (properties == null) {
    requestResult = this._database._connection.GET("/_api/collection/" + encodeURIComponent(this._id) + "/properties");

    TRI_CheckRequestResult(requestResult);
  }
  else {
    var body = {};

    if (properties.hasOwnProperty("waitForSync")) {
      body.waitForSync = properties.waitForSync;
    }

    requestResult = this._database._connection.PUT("/_api/collection/" + encodeURIComponent(this._id) + "/properties", JSON.stringify(body));

    TRI_CheckRequestResult(requestResult);
  }

  return { 
    waitForSync : requestResult.waitForSync,
    journalSize : requestResult.journalSize
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the figures of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.figures = function () {
  var requestResult = this._database._connection.GET("/_api/collection/" + encodeURIComponent(this._id) + "/figures");

  TRI_CheckRequestResult(requestResult);

  return requestResult.figures;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.drop = function () {
  var requestResult = this._database._connection.DELETE("/_api/collection/" + encodeURIComponent(this._id));

  TRI_CheckRequestResult(requestResult);

  this._status = AvocadoCollection.STATUS_DELETED;

  var database = this._database;

  for (var name in database) {
    if (database.hasOwnProperty(name)) {
      var collection = database[name];

      if (collection instanceof AvocadoCollection) {
        if (collection._id == this._id) {
          delete database[name];
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all indexes
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.getIndexes = function () {
  var requestResult = this._database._connection.GET("/_api/index?collection=" + encodeURIComponent(this._id));

  TRI_CheckRequestResult(requestResult);

  return requestResult.indexes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns one index
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.index = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var s = id.split("/");

  if (s.length != 2) {
    var requestResult = {
      errorNum: internal.errors.ERROR_AVOCADO_INDEX_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_AVOCADO_INDEX_HANDLE_BAD.message
    }

    throw new AvocadoError(requestResult);
  }
  else if (s[0] != this._id) {
    var requestResult = {
      errorNum: internal.errors.ERROR_AVOCADO_COLLECTION_NOT_FOUND.code,
      errorMessage: internal.errors.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.message
    }

    throw new AvocadoError(requestResult);
  }

  var requestResult = this._database._connection.GET("/_api/index/" + id);

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes one index
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.dropIndex = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var s = id.split("/");

  if (s.length != 2) {
    var requestResult = {
      errorNum: internal.errors.ERROR_AVOCADO_INDEX_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_AVOCADO_INDEX_HANDLE_BAD.message
    }

    throw new AvocadoError(requestResult);
  }
  else if (s[0] != this._id) {
    var requestResult = {
      errorNum: internal.errors.ERROR_AVOCADO_COLLECTION_NOT_FOUND.code,
      errorMessage: internal.errors.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.message
    }

    throw new AvocadoError(requestResult);
  }

  var requestResult = this._database._connection.DELETE("/_api/index/" + id);

  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_AVOCADO_INDEX_NOT_FOUND.code) {
    return false;
  }

  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an geo index
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.ensureGeoIndex = function (lat, lon) {
  var body;

  if (typeof lon == "boolean") {
    body = { type : "geo", fields : [ lat ], geoJson : lon, constraint : false };
  }
  else if (lon == undefined) {
    body = { type : "geo", fields : [ lat ], geoJson : false, constraint : false };
  }
  else {
    body = { type : "geo", fields : [ lat, lon ], constraint : false };
  }

  var requestResult = this._database._connection.POST("/_api/index?collection=" + encodeURIComponent(this._id), JSON.stringify(body));

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an geo constraint
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.ensureGeoConstraint = function (lat, lon, ignoreNull) {
  var body;

  // only two parameter
  if (ignoreNull == undefined) {
    ignoreNull = lon;

    if (typeof ignoreNull != "boolean") {
      throw "usage: ensureGeoConstraint(<lat>, <lon>, <ignore-null>) or ensureGeoConstraint(<lat>, <geo-json>, <ignore-null>)";
    }

    body = { type : "geo", fields : [ lat ], geoJson : false, constraint : true, ignoreNull : ignoreNull };
  }

  // three parameter
  else {
    if (typeof lon == "boolean") {
      body = { type : "geo", fields : [ lat ], geoJson : lon, constraint : true, ignoreNull : ignoreNull };
    }
    else {
      body = { type : "geo", fields : [ lat, lon ], constraint : true, ignoreNull : ignoreNull };
    }
  }

  var requestResult = this._database._connection.POST("/_api/index?collection=" + encodeURIComponent(this._id), JSON.stringify(body));

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.truncate = function () {
  var requestResult = this._database._connection.PUT("/_api/collection/" + encodeURIComponent(this._id) + "/truncate", "");

  TRI_CheckRequestResult(requestResult);

  this._status = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.load = function () {
  var requestResult = this._database._connection.PUT("/_api/collection/" + encodeURIComponent(this._id) + "/load", "");

  TRI_CheckRequestResult(requestResult);

  this._status = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.unload = function () {
  var requestResult = this._database._connection.PUT("/_api/collection/" + encodeURIComponent(this._id) + "/unload", "");

  TRI_CheckRequestResult(requestResult);

  this._status = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.rename = function (name) {
  var body = { name : name };
  var requestResult = this._database._connection.PUT("/_api/collection/" + encodeURIComponent(this._id) + "/rename", JSON.stringify(body));

  TRI_CheckRequestResult(requestResult);

  delete this._database[this._name];
  this._database[name] = this;

  this._status = null;
  this._name = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief refreshes a collection status and name
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.refresh = function () {
  var requestResult = this._database._connection.GET("/_api/collection/" + encodeURIComponent(this._id));

  TRI_CheckRequestResult(requestResult);

  this._name = requestResult['name'];
  this._status = requestResult['status'];
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of documents
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.count = function () {
  var requestResult = this._database._connection.GET("/_api/collection/" + encodeURIComponent(this._id) + "/count");

  TRI_CheckRequestResult(requestResult);

  return requestResult["count"];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single document from the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.document = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev == null) {
    requestResult = this._database._connection.GET("/document/" + id);
  }
  else {
    requestResult = this._database._connection.GET("/document/" + id, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_AVOCADO_COLLECTION_NOT_FOUND.code) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code;
    }
    else if (s[0] != this._id) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.code;
    }

    throw new AvocadoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.save = function (data) {    
  var requestResult = this._database._connection.POST("/document?collection=" + encodeURIComponent(this._id), JSON.stringify(data));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.remove = function (id, overwrite) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var policy = "";

  if (overwrite) {
    policy = "?policy=last";
  }

  if (rev == null) {
    requestResult = this._database._connection.DELETE("/document/" + id + policy);
  }
  else {
    requestResult = this._database._connection.DELETE("/document/" + id + policy, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code;
    }
    else if (s[0] != this._id) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.code;
    }

    if (overwrite) {
      if (requestResult.errorNum == internal.errors.ERROR_AVOCADO_DOCUMENT_NOT_FOUND.code) {
        return false;
      }
    }

    throw new AvocadoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.replace = function (id, data, overwrite) { 
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var policy = "";

  if (overwrite) {
    policy = "?policy=last";
  }

  if (rev == null) {
    requestResult = this._database._connection.PUT("/document/" + id + policy, JSON.stringify(data));
  }
  else {
    requestResult = this._database._connection.PUT("/document/" + id + policy, JSON.stringify(data), {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code;
    }
    else if (s[0] != this._id) {
      requestResult.errorNum = internal.errors.ERROR_AVOCADO_CROSS_COLLECTION_REQUEST.code;
    }

    throw new AvocadoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            AvocadoEdgesCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoEdgesCollection (database, data) {
  this._database = database;

  if (typeof data === "string") {
    this._name = data;
  }
  else {
    this._id = data.id;
    this._name = data.name;
    this._status = data.status;
  }
}

AvocadoEdgesCollection.prototype = new AvocadoCollection();

ModuleCache["/internal"].exports.AvocadoEdgesCollection = AvocadoEdgesCollection;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for AvocadoCollection
////////////////////////////////////////////////////////////////////////////////

helpAvocadoEdgesCollection = TRI_CreateHelpHeadline("AvocadoEdgesCollection help") +
'AvocadoEdgesCollection constructor:                                 ' + "\n" +
' > col = edges.mycoll;                                              ' + "\n" +
' > col = db._create("mycoll");                                      ' + "\n" +
'                                                                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                       database object                   ' + "\n" +
'  _id                             collection identifier             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the collection
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype.toString = function () {  
  return TRI_GetIdString(this, "AvocadoEdgesCollection");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the collection
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype._PRINT = function () {  
  var status = "unknown";

  switch (this.status()) {
    case AvocadoCollection.STATUS_NEW_BORN: status = "new born"; break;
    case AvocadoCollection.STATUS_UNLOADED: status = "unloaded"; break;
    case AvocadoCollection.STATUS_UNLOADING: status = "unloading"; break;
    case AvocadoCollection.STATUS_LOADED: status = "loaded"; break;
    case AvocadoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
    case AvocadoCollection.STATUS_DELETED: status = "deleted"; break;
  }
  
  internal.output("[AvocadoEdgesCollection ", this._id, ", \"", this.name(), "\" (status " + status + ")]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoCollection
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype._help = function () {  
  print(helpAvocadoEdgesCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype.save = function (from, to, data) {    
  if (from.hasOwnProperty("_id")) {
    from = from._id;
  }

  if (to.hasOwnProperty("_id")) {
    to = to._id;
  }

  var url = "/edge?collection=" + encodeURIComponent(this._id)
          + "&from=" + encodeURIComponent(from)
          + "&to=" + encodeURIComponent(to);

  var requestResult = this._database._connection.POST(url, JSON.stringify(data));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting or ending in a vertex
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype.edges = function (vertex) {

  // if vertex is a list, iterator and concat
  if (vertex instanceof Array) {
    var edges = [];

    for (var i = 0;  i < vertex.length;  ++i) {
      var e = this.edges(vertex[i]);
      
      edges.push.apply(edges, e);
    }

    return edges;
  }

  if (vertex.hasOwnProperty("_id")) {
    vertex = vertex._id;
  }

  // get the edges
  requestResult = this._database._connection.GET("/edges/" + encodeURIComponent(this._id) + "?vertex=" + encodeURIComponent(vertex));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult['edges'];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges ending in a vertex
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype.inEdges = function (vertex) {

  // if vertex is a list, iterator and concat
  if (vertex instanceof Array) {
    var edges = [];

    for (var i = 0;  i < vertex.length;  ++i) {
      var e = this.inEdges(vertex[i]);
      
      edges.push.apply(edges, e);
    }

    return edges;
  }

  if (vertex.hasOwnProperty("_id")) {
    vertex = vertex._id;
  }

  // get the edges
  requestResult = this._database._connection.GET("/edges/" + encodeURIComponent(this._id) + "?direction=in&vertex=" + encodeURIComponent(vertex));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult['edges'];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting in a vertex
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype.outEdges = function (vertex) {

  // if vertex is a list, iterator and concat
  if (vertex instanceof Array) {
    var edges = [];

    for (var i = 0;  i < vertex.length;  ++i) {
      var e = this.outEdges(vertex[i]);
      
      edges.push.apply(edges, e);
    }

    return edges;
  }

  if (vertex.hasOwnProperty("_id")) {
    vertex = vertex._id;
  }

  // get the edges
  requestResult = this._database._connection.GET("/edges/" + encodeURIComponent(this._id) + "?direction=out&vertex=" + encodeURIComponent(vertex));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult['edges'];
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                AvocadoQueryCursor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoQueryCursor (database, data) {
  this._database = database;
  this.data = data;
  this._hasNext = false;
  this._hasMore = false;
  this._pos = 0;
  this._count = 0;
  this._total = 0;
  
  if (data.result != undefined) {
    this._count = data.result.length;
    
    if (this._pos < this._count) {
      this._hasNext = true;
    }
    
    if (data.hasMore != undefined && data.hasMore) {
      this._hasMore = true;
    }    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for AvocadoQueryCursor
////////////////////////////////////////////////////////////////////////////////

helpAvocadoQueryCursor = TRI_CreateHelpHeadline("AvocadoQueryCursor help") +
'AvocadoQueryCursor constructor:                                     ' + "\n" +
' > cu1 = qi1.execute();                                             ' + "\n" +
'Functions:                                                          ' + "\n" +
'  hasMore();                            returns true if there       ' + "\n" +
'                                        are more results            ' + "\n" +
'  next();                               returns the next document   ' + "\n" +
'  elements();                           returns all documents       ' + "\n" +
'  _help();                              this help                   ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                             database object             ' + "\n" +
'Example:                                                            ' + "\n" +
' > st = db._createStatement({ "query" : "for c in coll return c" });' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > documents = c.elements();                                        ' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > while (c.hasNext()) { print( c.next() ); }                       ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for the cursor
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype._help = function () {
  print(helpAvocadoQueryCursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the cursor
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype.toString = function () {  
  return TRI_GetIdString(this, "AvocadoQueryCursor");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether there are more results available in the cursor
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype.hasNext = function () {
  return this._hasNext;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next result document from the cursor
///
/// If no more results are available locally but more results are available on
/// the server, this function will make a roundtrip to the server
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype.next = function () {
  if (!this._hasNext) {
    throw "No more results";
  }
 
  var result = this.data.result[this._pos];
  this._pos++;
    
  // reached last result
  if (this._pos == this._count) {
    this._hasNext = false;
    this._pos = 0;
    
    if (this._hasMore && this.data.id) {
      this._hasMore = false;
      
      // load more results      
      var requestResult = this._database._connection.PUT("/_api/cursor/"+ encodeURIComponent(this.data.id),  "");

      TRI_CheckRequestResult(requestResult);
      
      this.data = requestResult;
      this._count = requestResult.result.length;
    
      if (this._pos < this._count) {
        this._hasNext = true;
      }
    
      if (requestResult.hasMore != undefined && requestResult.hasMore) {
        this._hasMore = true;
      }                
    }    
  }
    
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all remaining result documents from the cursor
///
/// If no more results are available locally but more results are available on
/// the server, this function will make one or multiple roundtrips to the 
/// server. Calling this function will also fully exhaust the cursor.
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype.elements = function () {  
  var result = [];
  
  while (this.hasNext()) { 
    result.push( this.next() ); 
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly dispose the cursor
///
/// Calling this function will mark the cursor as deleted on the server. It will
/// therefore make a roundtrip to the server. Using a cursor after it has been
/// disposed is considered a user error
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype.dispose = function () {
  if (!this.data.id) {
    // client side only cursor
    return;
  }

  var requestResult = this._database._connection.DELETE("/_api/cursor/"+ encodeURIComponent(this.data.id), "");
    
  TRI_CheckRequestResult(requestResult);

  this.data.id = undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the total number of documents in the cursor
///
/// The number will remain the same regardless how much result documents have
/// already been fetched from the cursor.
///
/// This function will return the number only if the cursor was constructed 
/// with the "doCount" attribute. Otherwise it will return undefined.
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype.count = function () {
  if (!this.data.id) {
    throw "cursor has been disposed";
  }

  return this.data.count;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  AvocadoStatement
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoStatement (database, data) {
  this._database = database;
  this._doCount = false;
  this._batchSize = null;
  this._bindVars = {};
  
  if (!(data instanceof Object)) {
    throw "AvocadoStatement needs initial data";
  }
    
  if (data.query == undefined || data.query == "") {
    throw "AvocadoStatement needs a valid query attribute";
  }
  this.setQuery(data.query);

  if (data.count != undefined) {
    this.setCount(data.count);
  }
  if (data.batchSize != undefined) {
    this.setBatchSize(data.batchSize);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for AvocadoStatement
////////////////////////////////////////////////////////////////////////////////

helpAvocadoStatement = TRI_CreateHelpHeadline("AvocadoStatement help") +
'AvocadoStatement constructor:                                       ' + "\n" +
' > st = new AvocadoStatement({ "query" : "for ..." });              ' + "\n" +
' > st = db._createStatement({ "query" : "for ..." });               ' + "\n" +
'Functions:                                                          ' + "\n" +
'  bind(<key>, <value>);          bind single variable               ' + "\n" +
'  bind(<values>);                bind multiple variables            ' + "\n" +
'  setBatchSize(<max>);           set max. number of results         ' + "\n" +
'                                 to be transferred per roundtrip    ' + "\n" +
'  setCount(<value>);             set count flag (return number of   ' + "\n" +
'                                 results in "count" attribute)      ' + "\n" +
'  getBatchSize();                return max. number of results      ' + "\n" +
'                                 to be transferred per roundtrip    ' + "\n" +
'  getCount();                    return count flag (return number of' + "\n" +
'                                 results in "count" attribute)      ' + "\n" +
'  getQuery();                    return query string                ' + "\n" +
'  execute();                     execute query and return cursor    ' + "\n" +
'  _help();                       this help                          ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                      database object                    ' + "\n" +
'Example:                                                            ' + "\n" +
' > st = db._createStatement({ "query" : "for c in coll filter       ' + "\n" +
'                              c.x = @a@ && c.y = @b@ return c" });  ' + "\n" +
' > st.bind("a", "hello");                                           ' + "\n" +
' > st.bind("b", "world");                                           ' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > print(c.elements());                                             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoStatement
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype._help = function () {
  print(helpAvocadoStatement);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the statement
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.toString = function () {  
  return TRI_GetIdString(this, "AvocadoStatement");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief bind a parameter to the statement
///
/// This function can be called multiple times, once for each bind parameter.
/// All bind parameters will be transferred to the server in one go when 
/// execute() is called.
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.bind = function (key, value) {
  if (key instanceof Object) {
    if (value != undefined) {
      throw "invalid bind parameter declaration";
    }
    this._bindVars = key;
  }
  else if (typeof(key) == "string") {
    if (this._bindVars[key] != undefined) {
      throw "redeclaration of bind parameter";
    }
    this._bindVars[key] = value;
  }
  else if (typeof(key) == "number") {
    var strKey = String(parseInt(key));
    if (strKey != String(key)) {
      throw "invalid bind parameter declaration";
    }
    if (this._bindVars[strKey] != undefined) {
      throw "redeclaration of bind parameter";
    }
    this._bindVars[strKey] = value;
  }
  else {
    throw "invalid bind parameter declaration";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the bind variables already set
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.getBindVariables = function () {
  return this._bindVars;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the count flag for the statement
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.getCount = function () {
  return this._doCount;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the maximum number of results documents the cursor will return
/// in a single server roundtrip.
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.getBatchSize = function () {
  return this._batchSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get query string
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.getQuery = function () {
  return this._query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the count flag for the statement
///
/// Setting the count flag will make the statement's result cursor return the
/// total number of result documents. The count flag is not set by default.
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.setCount = function (bool) {
  this._doCount = bool ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the maximum number of results documents the cursor will return
/// in a single server roundtrip.
/// The higher this number is, the less server roundtrips will be made when
/// iterating over the result documents of a cursor.
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.setBatchSize = function (value) {
  if (parseInt(value) > 0) {
    this._batchSize = parseInt(value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query string
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.setQuery = function (query) {
  this._query = query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return the results
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.parse = function () {
  var body = {
    "query" : this._query,
  }

  var requestResult = this._database._connection.POST("/_api/query", JSON.stringify(body));
    
  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// Invoking execute() will transfer the query and all bind parameters to the
/// server. It will return a cursor with the query results in case of success.
/// In case of an error, the error will be printed
////////////////////////////////////////////////////////////////////////////////

AvocadoStatement.prototype.execute = function () {
  var body = {
    "query" : this._query,
    "count" : this._doCount,
    "bindVars" : this._bindVars
  }

  if (this._batchSize) {
    body["batchSize"] = this._batchSize;
  }

  var requestResult = this._database._connection.POST("/_api/cursor", JSON.stringify(body));
    
  TRI_CheckRequestResult(requestResult);

  return new AvocadoQueryCursor(this._database, requestResult);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      initialisers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief general help
////////////////////////////////////////////////////////////////////////////////

HELP = TRI_CreateHelpHeadline("Help") +
'Predefined objects:                                                 ' + "\n" +
'  avocado:                               AvocadoConnection          ' + "\n" +
'  db:                                    AvocadoDatabase            ' + "\n" +
'Example:                                                            ' + "\n" +
' > db._collections();                    list all collections       ' + "\n" +
' > db.<coll_name>.all();                 list all documents         ' + "\n" +
' > id = db.<coll_name>.save({ ... });    save a document            ' + "\n" +
' > db.<coll_name>.remove(<_id>);         delete a document          ' + "\n" +
' > db.<coll_name>.document(<_id>);       get a document             ' + "\n" +
' > help                                  show help pages            ' + "\n" +
' > helpQueries                           query help                 ' + "\n" +
' > exit                                                             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief query help
////////////////////////////////////////////////////////////////////////////////

helpQueries = TRI_CreateHelpHeadline("Select query help") +
'Create a select query:                                              ' + "\n" +
' > st = new AvocadoStatement(db, { "query" : "for..." });           ' + "\n" +
' > st = db._createStatement({ "query" : "for..." });                ' + "\n" +
'Set query options:                                                  ' + "\n" +
' > st.setBatchSize(<value>);     set the max. number of results     ' + "\n" +
'                                 to be transferred per roundtrip    ' + "\n" +
' > st.setCount(<value>);         set count flag (return number of   ' + "\n" +
'                                 results in "count" attribute)      ' + "\n" +
'Get query options:                                                  ' + "\n" +
' > st.setBatchSize();            return the max. number of results  ' + "\n" +
'                                 to be transferred per roundtrip    ' + "\n" +
' > st.getCount();                return count flag (return number of' + "\n" +
'                                 results in "count" attribute)      ' + "\n" +
' > st.getQuery();                return query string                ' + "\n" +
'                                 results in "count" attribute)      ' + "\n" +
'Bind parameters to a query:                                         ' + "\n" +
' > st.bind(<key>, <value>);      bind single variable               ' + "\n" +
' > st.bind(<values>);            bind multiple variables            ' + "\n" +
'Execute query:                                                      ' + "\n" +
' > c = st.execute();             returns a cursor                   ' + "\n" +
'Get all results in an array:                                        ' + "\n" +
' > e = c.elements();                                                ' + "\n" +
'Or loop over the result set:                                        ' + "\n" +
' > while (c.hasNext()) { print( c.next() ); }                       ';

////////////////////////////////////////////////////////////////////////////////
/// @brief extended help
////////////////////////////////////////////////////////////////////////////////

helpExtended = TRI_CreateHelpHeadline("More help") +
'Pager:                                                              ' + "\n" +
' > internal.stop_pager()               stop the pager output        ' + "\n" +
' > internal.start_pager()              start the pager              ' + "\n" +
'Pretty printing:                                                    ' + "\n" +
' > stop_pretty_print()                 stop pretty printing         ' + "\n" +
' > start_pretty_print()                start pretty printing        ' + "\n" +
'Color output:                                                       ' + "\n" +
' > stop_color_print()                  stop color printing          ' + "\n" +
' > start_color_print()                 start color printing         ' + "\n" +
' > start_color_print(COLOR_BLUE)       set color                    ' + "\n" +
'Print function:                                                     ' + "\n" +
' > print(x)                            std. print function          ' + "\n" +
' > print_plain(x)                      print without pretty printing' + "\n" +
'                                       and without colors           ';

////////////////////////////////////////////////////////////////////////////////
/// @brief create the global db object and load the collections
////////////////////////////////////////////////////////////////////////////////

try {

  if (typeof avocado !== 'undefined') {

    // default databases
    db = new AvocadoDatabase(avocado);
    edges = new AvocadoEdges(avocado);

    // load collection data
    db._collections();
    edges._collections();

    // export to internal
    ModuleCache["/internal"].exports.db = db;
    ModuleCache["/internal"].exports.edges = db;

    print(HELP);
  }
}
catch (err) {
  print(err);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
