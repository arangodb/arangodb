////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API
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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
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
var helpArangoDatabase = "";
var helpArangoCollection = "";
var helpArangoQueryCursor = "";
var helpArangoStoredStatement = "";
var helpArangoStatement = "";
var helpExtended = "";

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
    throw new ArangoError(requestResult);
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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief turn off pretty print2ing
////////////////////////////////////////////////////////////////////////////////

function print2_plain (data) {
  var p = PRETTY_PRINT;
  PRETTY_PRINT = false;
  var c;
  if (typeof(COLOR_OUTPUT) != undefined) {
    c = COLOR_OUTPUT;
    COLOR_OUTPUT = undefined;
  }
  
  try {
    print2(data);
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
/// @brief start pretty print2ing
////////////////////////////////////////////////////////////////////////////////

function start_pretty_print2 () {
  print2("use pretty print2ing");
  PRETTY_PRINT=true;
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty print2ing
////////////////////////////////////////////////////////////////////////////////

function stop_pretty_print2 () {
  print2("stop pretty print2ing");
  PRETTY_PRINT=false;
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty print2ing with optional color
////////////////////////////////////////////////////////////////////////////////

function start_color_print2 (color) {
  if (typeof(color) == "string") {
    COLOR_OUTPUT = color;
  }
  else {
    COLOR_OUTPUT = COLOR_BRIGHT;
  }
  print2("start " + COLOR_OUTPUT + "color" + COLOR_OUTPUT_RESET + " print2ing");
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty print2ing
////////////////////////////////////////////////////////////////////////////////

function stop_color_print2 () {
  print2("stop color print2ing");
  COLOR_OUTPUT = undefined;
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print2 the overall help
////////////////////////////////////////////////////////////////////////////////

function help () {
  print2(HELP);
  print2(helpQueries);
  print2(helpArangoDatabase);
  print2(helpArangoCollection);
  print2(helpArangoStatement);
  print2(helpArangoQueryCursor);
  print2(helpExtended);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief change internal.output to shell output
////////////////////////////////////////////////////////////////////////////////
/*
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
*/
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      ArangoError
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

function ArangoError (error) {
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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief print2s an error
////////////////////////////////////////////////////////////////////////////////

ArangoError.prototype._PRINT = function() {
  internal.output(this.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toString function
////////////////////////////////////////////////////////////////////////////////

ArangoError.prototype.toString = function() {
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
/// @brief print2s an error
////////////////////////////////////////////////////////////////////////////////

ArangoError.prototype.toString = function() {
  var errorNum = this.errorNum;
  var errorMessage = this.errorMessage;

  return "[ArangoError " + errorNum + ": " + errorMessage + "]";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoDatabase
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

function ArangoDatabase (connection) {
  this._connection = connection;
  this._collectionConstructor = ArangoCollection;
}

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
/// @brief help for ArangoDatabase
////////////////////////////////////////////////////////////////////////////////

helpArangoDatabase = TRI_CreateHelpHeadline("ArangoDatabase help") +
'ArangoDatabase constructor:                                        ' + "\n" +
' > db = new ArangoDatabase(connection);                            ' + "\n" +
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
'  _delete(<id>)                   deletes document                  ' + "\n" +
'                                                                    ' + "\n" +
'Query Functions:                                                    ' + "\n" +
'  _createStatement(<data>);      create and return select query     ' + "\n" +
'                                                                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  <collection names>             collection with the given name     ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print2 the help for ArangoDatabase
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._help = function () {  
  print2(helpArangoDatabase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the database object
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype.toString = function () {  
  return "[object ArangoDatabase]";
}

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
/// @brief return all collections from the database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._collections = function () {
  var requestResult = this._connection.get("/_api/collection");
  
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

ArangoDatabase.prototype._collection = function (id) {
  var requestResult = this._connection.get("/_api/collection/" + encodeURIComponent(id));
  
  // return null in case of not found
  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
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

ArangoDatabase.prototype._create = function (name, properties) {
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

  var requestResult = this._connection.post("/_api/collection", JSON.stringify(body));

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

ArangoDatabase.prototype._truncate = function (id) {
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

ArangoDatabase.prototype._drop = function (id) {
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
/// @brief return a single document from the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._document = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev == null) {
    requestResult = this._connection.get("/document/" + id);
  }
  else {
    requestResult = this._connection.get("/document/" + id, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    }

    throw new ArangoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._delete = function (id, overwrite) {
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
    requestResult = this._connection.delete("/document/" + id + policy);
  }
  else {
    requestResult = this._connection.delete("/document/" + id + policy, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    }

    if (overwrite) {
      if (requestResult.errorNum == internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        return false;
      }
    }

    throw new ArangoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._replace = function (id, data, overwrite) { 
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
    requestResult = this._connection.put("/document/" + id + policy, JSON.stringify(data));
  }
  else {
    requestResult = this._connection.put("/document/" + id + policy, JSON.stringify(data), {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    }

    throw new ArangoError(requestResult);
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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create a new statement
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createStatement = function (data) {  
  return new ArangoStatement(this, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      ArangoEdges
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

function ArangoEdges (connection) {
  this._connection = connection;
  this._collectionConstructor = ArangoEdgesCollection;
}

ArangoEdges.prototype = new ArangoDatabase();

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
/// @brief help for ArangoEdges
////////////////////////////////////////////////////////////////////////////////

helpArangoEdges = TRI_CreateHelpHeadline("ArangoEdges help") +
'ArangoEdges constructor:                                           ' + "\n" +
' > edges = new ArangoEdges(connection);                            ' + "\n" +
'                                                                    ' + "\n" +
'Administration Functions:                                           ' + "\n" +
'  _help();                       this help                          ' + "\n" +
'                                                                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  <collection names>             collection with the given name     ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print2 the help for ArangoEdges
////////////////////////////////////////////////////////////////////////////////

ArangoEdges.prototype._help = function () {  
  print2(helpArangoEdges);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the database object
////////////////////////////////////////////////////////////////////////////////

ArangoEdges.prototype.toString = function () {  
  return "[object ArangoEdges]";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 ArangoCollection
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

function ArangoCollection (database, data) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
/// @brief help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

helpArangoCollection = TRI_CreateHelpHeadline("ArangoCollection help") +
'ArangoCollection constructor:                                      ' + "\n" +
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

ArangoCollection.prototype.toString = function () {  
  return TRI_GetIdString(this, "ArangoCollection");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print2s the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._PRINT = function () {  
  var status = "unknown";

  switch (this.status()) {
    case ArangoCollection.STATUS_NEW_BORN: status = "new born"; break;
    case ArangoCollection.STATUS_UNLOADED: status = "unloaded"; break;
    case ArangoCollection.STATUS_UNLOADING: status = "unloading"; break;
    case ArangoCollection.STATUS_LOADED: status = "loaded"; break;
    case ArangoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
    case ArangoCollection.STATUS_DELETED: status = "deleted"; break;
  }
  
  internal.output("[ArangoCollection ", this._id, ", \"", this.name(), "\" (status " + status + ")]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print2 the help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._help = function () {  
  print2(helpArangoCollection);
}

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
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.name = function () {
  if (this._name == null) {
    this.refresh();
  }

  return this._name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.status = function () {
  if (this._status == null) {
    this.refresh();
  }

  return this._status;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.properties = function (properties) {
  var requestResult;

  if (properties == null) {
    requestResult = this._database._connection.get("/_api/collection/" + encodeURIComponent(this._id) + "/properties");

    TRI_CheckRequestResult(requestResult);
  }
  else {
    var body = {};

    if (properties.hasOwnProperty("waitForSync")) {
      body.waitForSync = properties.waitForSync;
    }

    requestResult = this._database._connection.put("/_api/collection/" + encodeURIComponent(this._id) + "/properties", JSON.stringify(body));

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

ArangoCollection.prototype.figures = function () {
  var requestResult = this._database._connection.get("/_api/collection/" + encodeURIComponent(this._id) + "/figures");

  TRI_CheckRequestResult(requestResult);

  return requestResult.figures;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.drop = function () {
  var requestResult = this._database._connection.delete("/_api/collection/" + encodeURIComponent(this._id));

  TRI_CheckRequestResult(requestResult);

  this._status = ArangoCollection.STATUS_DELETED;

  var database = this._database;

  for (var name in database) {
    if (database.hasOwnProperty(name)) {
      var collection = database[name];

      if (collection instanceof ArangoCollection) {
        if (collection._id == this._id) {
          delete database[name];
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
  var requestResult = this._database._connection.put("/_api/collection/" + encodeURIComponent(this._id) + "/truncate", "");

  TRI_CheckRequestResult(requestResult);

  this._status = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.load = function () {
  var requestResult = this._database._connection.put("/_api/collection/" + encodeURIComponent(this._id) + "/load", "");

  TRI_CheckRequestResult(requestResult);

  this._status = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.unload = function () {
  var requestResult = this._database._connection.put("/_api/collection/" + encodeURIComponent(this._id) + "/unload", "");

  TRI_CheckRequestResult(requestResult);

  this._status = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.rename = function (name) {
  var body = { name : name };
  var requestResult = this._database._connection.put("/_api/collection/" + encodeURIComponent(this._id) + "/rename", JSON.stringify(body));

  TRI_CheckRequestResult(requestResult);

  delete this._database[this._name];
  this._database[name] = this;

  this._status = null;
  this._name = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief refreshes a collection status and name
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.refresh = function () {
  var requestResult = this._database._connection.get("/_api/collection/" + encodeURIComponent(this._id));

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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of documents
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.count = function () {
  var requestResult = this._database._connection.get("/_api/collection/" + encodeURIComponent(this._id) + "/count");

  TRI_CheckRequestResult(requestResult);

  return requestResult["count"];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all documents from the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.all = function () {
  var data = {
    collection : this._id
  }  
  
  var requestResult = this._database._connection.put("/_api/simple/all", JSON.stringify(data));

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single document from the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.document = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev == null) {
    requestResult = this._database._connection.get("/document/" + id);
  }
  else {
    requestResult = this._database._connection.get("/document/" + id, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    }
    else if (s[0] != this._id) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code;
    }

    throw new ArangoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.save = function (data) {    
  var requestResult = this._database._connection.post("/document?collection=" + encodeURIComponent(this._id), JSON.stringify(data));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.delete = function (id, overwrite) {
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
    requestResult = this._database._connection.delete("/document/" + id + policy);
  }
  else {
    requestResult = this._database._connection.delete("/document/" + id + policy, {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    }
    else if (s[0] != this._id) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code;
    }

    if (overwrite) {
      if (requestResult.errorNum == internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        return false;
      }
    }

    throw new ArangoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replace = function (id, data, overwrite) { 
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
    requestResult = this._database._connection.put("/document/" + id + policy, JSON.stringify(data));
  }
  else {
    requestResult = this._database._connection.put("/document/" + id + policy, JSON.stringify(data), {'if-match' : '"' + rev + '"' });
  }

  if (requestResult != null && requestResult.error == true) {
    var s = id.split("/");

    if (s.length != 2) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    }
    else if (s[0] != this._id) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code;
    }

    throw new ArangoError(requestResult);
  }

  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            ArangoEdgesCollection
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

function ArangoEdgesCollection (database, data) {
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

ArangoEdgesCollection.prototype = new ArangoCollection();

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
/// @brief help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

helpArangoEdgesCollection = TRI_CreateHelpHeadline("ArangoEdgesCollection help") +
'ArangoEdgesCollection constructor:                                 ' + "\n" +
' > col = edges.mycoll;                                              ' + "\n" +
' > col = db._create("mycoll");                                      ' + "\n" +
'                                                                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                       database object                   ' + "\n" +
'  _id                             collection identifier             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the collection
////////////////////////////////////////////////////////////////////////////////

ArangoEdgesCollection.prototype.toString = function () {  
  return TRI_GetIdString(this, "ArangoEdgesCollection");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print2s the collection
////////////////////////////////////////////////////////////////////////////////

ArangoEdgesCollection.prototype._PRINT = function () {  
  var status = "unknown";

  switch (this.status()) {
    case ArangoCollection.STATUS_NEW_BORN: status = "new born"; break;
    case ArangoCollection.STATUS_UNLOADED: status = "unloaded"; break;
    case ArangoCollection.STATUS_UNLOADING: status = "unloading"; break;
    case ArangoCollection.STATUS_LOADED: status = "loaded"; break;
    case ArangoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
    case ArangoCollection.STATUS_DELETED: status = "deleted"; break;
  }
  
  internal.output("[ArangoEdgesCollection ", this._id, ", \"", this.name(), "\" (status " + status + ")]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print2 the help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

ArangoEdgesCollection.prototype._help = function () {  
  print2(helpArangoEdgesCollection);
}

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
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

ArangoEdgesCollection.prototype.save = function (from, to, data) {    
  if (from.hasOwnProperty("_id")) {
    from = from._id;
  }

  if (to.hasOwnProperty("_id")) {
    to = to._id;
  }

  var url = "/edge?collection=" + encodeURIComponent(this._id)
          + "&from=" + encodeURIComponent(from)
          + "&to=" + encodeURIComponent(to);

  var requestResult = this._database._connection.post(url, JSON.stringify(data));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting or ending in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoEdgesCollection.prototype.edges = function (vertex) {

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
  requestResult = this._database._connection.get("/edges/" + encodeURIComponent(this._id) + "?vertex=" + encodeURIComponent(vertex));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult['edges'];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges ending in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoEdgesCollection.prototype.inEdges = function (vertex) {

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
  requestResult = this._database._connection.get("/edges/" + encodeURIComponent(this._id) + "?direction=in&vertex=" + encodeURIComponent(vertex));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult['edges'];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoEdgesCollection.prototype.outEdges = function (vertex) {

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
  requestResult = this._database._connection.get("/edges/" + encodeURIComponent(this._id) + "?direction=out&vertex=" + encodeURIComponent(vertex));
  
  TRI_CheckRequestResult(requestResult);

  return requestResult['edges'];
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                ArangoQueryCursor
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

function ArangoQueryCursor (database, data) {
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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for ArangoQueryCursor
////////////////////////////////////////////////////////////////////////////////

helpArangoQueryCursor = TRI_CreateHelpHeadline("ArangoQueryCursor help") +
'ArangoQueryCursor constructor:                                     ' + "\n" +
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
' > st = db._createStatement({ "query" : "select a from colA a" });  ' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > documents = c.elements();                                        ' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > while (c.hasNext()) { print2( c.next() ); }                       ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print2 the help for the cursor
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype._help = function () {
  print2(helpArangoQueryCursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the cursor
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.toString = function () {  
  return TRI_GetIdString(this, "ArangoQueryCursor");
}

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
/// @brief return whether there are more results available in the cursor
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.hasNext = function () {
  return this._hasNext;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next result document from the cursor
///
/// If no more results are available locally but more results are available on
/// the server, this function will make a roundtrip to the server
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.next = function () {
  if (!this._hasNext) {
    throw "No more results";
  }
 
  var result = this.data.result[this._pos];
  this._pos++;
    
  // reached last result
  if (this._pos == this._count) {
    this._hasNext = false;
    this._pos = 0;
    
    if (this._hasMore && this.data._id) {
      this._hasMore = false;
      
      // load more results      
      var requestResult = this._database._connection.put("/_api/cursor/"+ encodeURIComponent(this.data._id),  "");
    
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

ArangoQueryCursor.prototype.elements = function () {  
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

ArangoQueryCursor.prototype.dispose = function () {
  if (!this.data._id) {
    // client side only cursor
    return;
  }

  var requestResult = this._database._connection.delete("/_api/cursor/"+ encodeURIComponent(this.data._id), "");
    
  TRI_CheckRequestResult(requestResult);

  this.data._id = undefined;
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

ArangoQueryCursor.prototype.count = function () {
  if (!this.data._id) {
    throw "cursor has been disposed";
  }

  return this.data.count;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoStatement
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

function ArangoStatement (database, data) {
  this._database = database;
  this._doCount = false;
  this._batchSize = null;
  this._bindVars = {};
  
  if (!(data instanceof Object)) {
    throw "ArangoStatement needs initial data";
  }
    
  if (data.query == undefined || data.query == "") {
    throw "ArangoStatement needs a valid query attribute";
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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help for ArangoStatement
////////////////////////////////////////////////////////////////////////////////

helpArangoStatement = TRI_CreateHelpHeadline("ArangoStatement help") +
'ArangoStatement constructor:                                       ' + "\n" +
' > st = new ArangoStatement({ "query" : "select..." });            ' + "\n" +
' > st = db._createStatement({ "query" : "select ...." });           ' + "\n" +
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
' > st = db._createStatement({ "query" : "select a from colA a       ' + "\n" +
'                              where a.x = @a@ and a.y = @b@" });    ' + "\n" +
' > st.bind("a", "hello");                                           ' + "\n" +
' > st.bind("b", "world");                                           ' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > print2(c.elements());                                             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print2 the help for ArangoStatement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype._help = function () {
  print2(helpArangoStatement);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the statement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.toString = function () {  
  return TRI_GetIdString(this, "ArangoStatement");
}

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
/// @brief bind a parameter to the statement
///
/// This function can be called multiple times, once for each bind parameter.
/// All bind parameters will be transferred to the server in one go when 
/// execute() is called.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.bind = function (key, value) {
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

ArangoStatement.prototype.getBindVariables = function () {
  return this._bindVars;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the count flag for the statement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getCount = function () {
  return this._doCount;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the maximum number of results documents the cursor will return
/// in a single server roundtrip.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getBatchSize = function () {
  return this._batchSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get query string
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getQuery = function () {
  return this._query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the count flag for the statement
///
/// Setting the count flag will make the statement's result cursor return the
/// total number of result documents. The count flag is not set by default.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setCount = function (bool) {
  this._doCount = bool ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the maximum number of results documents the cursor will return
/// in a single server roundtrip.
/// The higher this number is, the less server roundtrips will be made when
/// iterating over the result documents of a cursor.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setBatchSize = function (value) {
  if (parseInt(value) > 0) {
    this._batchSize = parseInt(value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query string
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setQuery = function (query) {
  this._query = query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.parse = function () {
  var body = {
    "query" : this._query,
  }

  var requestResult = this._database._connection.post("/_api/query", JSON.stringify(body));
    
  TRI_CheckRequestResult(requestResult);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// Invoking execute() will transfer the query and all bind parameters to the
/// server. It will return a cursor with the query results in case of success.
/// In case of an error, the error will be print2ed
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.execute = function () {
  var body = {
    "query" : this._query,
    "count" : this._doCount,
    "bindVars" : this._bindVars
  }

  if (this._batchSize) {
    body["batchSize"] = this._batchSize;
  }

  var requestResult = this._database._connection.post("/_api/cursor", JSON.stringify(body));
    
  TRI_CheckRequestResult(requestResult);

  return new ArangoQueryCursor(this._database, requestResult);
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
'  arango:                               ArangoConnection          ' + "\n" +
'  db:                                    ArangoDatabase            ' + "\n" +
'Example:                                                            ' + "\n" +
' > db._collections();                    list all collections       ' + "\n" +
' > db.<coll_name>.all();                 list all documents         ' + "\n" +
' > id = db.<coll_name>.save({ ... });    save a document            ' + "\n" +
' > db.<coll_name>.delete(<_id>);         delete a document          ' + "\n" +
' > db.<coll_name>.document(<_id>);       get a document             ' + "\n" +
' > help                                  show help pages            ' + "\n" +
' > helpQueries                           query help                 ' + "\n" +
' > exit                                                             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief query help
////////////////////////////////////////////////////////////////////////////////

helpQueries = TRI_CreateHelpHeadline("Select query help") +
'Create a select query:                                              ' + "\n" +
' > st = new ArangoStatement(db, { "query" : "select..." });        ' + "\n" +
' > st = db._createStatement({ "query" : "select..." });             ' + "\n" +
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
' > while (c.hasNext()) { print2( c.next() ); }                       ';

////////////////////////////////////////////////////////////////////////////////
/// @brief extended help
////////////////////////////////////////////////////////////////////////////////

helpExtended = TRI_CreateHelpHeadline("More help") +
'Pager:                                                              ' + "\n" +
' > internal.stop_pager()               stop the pager output        ' + "\n" +
' > internal.start_pager()              start the pager              ' + "\n" +
'Pretty print2ing:                                                    ' + "\n" +
' > stop_pretty_print2()                 stop pretty print2ing         ' + "\n" +
' > start_pretty_print2()                start pretty print2ing        ' + "\n" +
'Color output:                                                       ' + "\n" +
' > stop_color_print2()                  stop color print2ing          ' + "\n" +
' > start_color_print2()                 start color print2ing         ' + "\n" +
' > start_color_print2(COLOR_BLUE)       set color                    ' + "\n" +
'Print function:                                                     ' + "\n" +
' > print2(x)                            std. print2 function          ' + "\n" +
' > print2_plain(x)                      print2 without pretty print2ing' + "\n" +
'                                       and without colors           ';

////////////////////////////////////////////////////////////////////////////////
/// @brief create the global db object and load the collections
////////////////////////////////////////////////////////////////////////////////

try {

  // default databases
  db = new ArangoDatabase(arango);
  edges = new ArangoEdges(arango);

  // load collection data
  db._collections();
  edges._collections();

  // export to internal
  ModuleCache["/internal"].exports.db = db;
  ModuleCache["/internal"].exports.edges = db;

  print2(HELP);
}
catch (err) {
  console.log(err);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
