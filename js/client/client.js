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
/// @brief change internal.output to shell output
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"].exports.output = TRI_SYS_OUTPUT;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a formatted type string for object
/// 
/// if the object has an id, it will be included in the string
////////////////////////////////////////////////////////////////////////////////

function getIdString (object, typeName) {
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
/// @brief turn off pretty printing
////////////////////////////////////////////////////////////////////////////////

function printPlain (data) {
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
/// @brief handle error results
/// 
/// return try if the result contains an error. in this case, the function will
/// also print the error details
////////////////////////////////////////////////////////////////////////////////

function isErrorResult (requestResult) {
  if (requestResult == undefined) {    
    requestResult = {
      "error" : true,
      "code"  : 0,
      "errorNum" : 0,
      "errorMessage" : "Unknown error. Request result is empty"
    }    
  }
  
  if (requestResult["error"] != undefined && requestResult["error"]) {    
    var code         = requestResult["code"];
    var errorNum     = requestResult["errorNum"];
    var errorMessage = requestResult["errorMessage"];

    if ( typeof(COLOR_BRIGHT) != "undefined" ) {
      internal.output(COLOR_BRIGHT);
      internal.output("Error: ");
      internal.output(COLOR_OUTPUT_RESET);
    }
    else  {
      internal.output("Error: ");      
    }

    internal.output("["); 
    internal.output(code); 
    internal.output(":"); 
    internal.output(errorNum); 
    internal.output("] "); 
    print(errorMessage);
    
    return true;
  }  
  return false;
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

function stop_color_print () {
  print("stop color printing");
  COLOR_OUTPUT = undefined;
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
/// @brief create a formatted headline text 
////////////////////////////////////////////////////////////////////////////////

function getHeadline (text) {
  var x = parseInt(Math.abs(78 - text.length) / 2);
  
  var p = "";
  for (var i = 0; i < x; ++i) {
    p += "-";
  }
  
  return "\n" + p + " " + text + " " + p + "\n";
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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
// --SECTION--                                                 AvocadoCollection
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoCollection (database, data) {
  this._database = database;

  if (typeof data === "string") {
    this.name = data;
  }
  else {
    for (var i in data) {
      this[i] = data[i];
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all documents from the collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.all = function () {
  var requestResult = this._database._connection.get("/_api/documents/" + encodeURIComponent(this.name));
    
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["documents"];    
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single document from the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.document = function (id) {
  var requestResult = this._database._connection.get("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));

  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["document"];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.save = function (data) {    
  var requestResult = this._database._connection.post("/_api/document/" + encodeURIComponent(this.name), JSON.stringify(data));
  
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["_id"];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.delete = function (id) {    
  var requestResult = this._database._connection.delete("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));

  return !isErrorResult(requestResult);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.update = function (id, data) {    
  var requestResult = this._database._connection.put("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id), JSON.stringify(data));

  return !isErrorResult(requestResult);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoCollection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype._help = function () {  
  print(helpAvocadoCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.toString = function () {  
  return getIdString(this, "AvocadoCollection");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                AvocadoQueryCursor
// -----------------------------------------------------------------------------

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
    
  if (this._pos == this._count) {
    // reached last result
    
    this._hasNext = false;
    this._pos = 0;
    
    if (this._hasMore && this.data._id) {
      this._hasMore = false;
      
      // load more results      
      var requestResult = this._database._connection.put("/_api/cursor/"+ encodeURIComponent(this.data._id),  "");
    
      if (isErrorResult(requestResult)) {
        return undefined;
      }
      
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
  if (!this.data._id) {
    // client side only cursor
    return;
  }

  var requestResult = this._database._connection.delete("/_api/cursor/"+ encodeURIComponent(this.data._id), "");
    
  if (!isErrorResult(requestResult)) {
    this.data._id = undefined;
  }
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
  if (!this.data._id) {
    throw "cursor has been disposed";
  }

  return this.data.count;
}

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
  return getIdString(this, "AvocadoQueryCursor");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   AvocadoDatabase
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoDatabase (connection) {
  this._connection = connection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all collections from the database
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._collections = function () {
  var requestResult = this._connection.get("/_api/collections");
  
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  if (requestResult["collections"] != undefined) {
    
    // add all collentions to object
    for (var i in requestResult["collections"]) {
      this[i] = new AvocadoCollection(this, requestResult["collections"][i]);
    }
      
    return requestResult["collections"];
  }
  
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._collection = function (id) {
  var requestResult = this._connection.get("/_api/collection/" + encodeURIComponent(id));
  
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  if (requestResult["name"] != undefined) {
    
    this[requestResult["name"]] = new AvocadoCollection(this, requestResult);
      
    return requestResult;
  }
  
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._create = function (name) {
  var body = {
    "name" : name
  };

  var requestResult = this._connection.post("/_api/collection", JSON.stringify(body));

  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult;
}

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  AvocadoStatement
// -----------------------------------------------------------------------------

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

  var requestResult = this._database._connection.post("/_api/query", JSON.stringify(body));
    
  if (isErrorResult(requestResult)) {
    return undefined;
  }

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

  var requestResult = this._database._connection.post("/_api/cursor", JSON.stringify(body));
    
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return new AvocadoQueryCursor(this._database, requestResult);
}

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
  return getIdString(this, "AvocadoStatement");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create a new statement
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._createStatement = function (data) {  
  return new AvocadoStatement(this, data);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      initialisers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise help texts
////////////////////////////////////////////////////////////////////////////////

HELP = 
getHeadline("Help") +
'Predefined objects:                                                 ' + "\n" +
'  avocado:                               AvocadoConnection          ' + "\n" +
'  db:                                    AvocadoDatabase            ' + "\n" +
'Example:                                                            ' + "\n" +
' > db._collections();                    list all collections       ' + "\n" +
' > db.<coll_name>.all();                 list all documents         ' + "\n" +
' > id = db.<coll_name>.save({ ... });    save a document            ' + "\n" +
' > db.<coll_name>.delete(<_id>);         delete a document          ' + "\n" +
' > db.<coll_name>.document(<_id>);       get a document             ' + "\n" +
' > help                                  show help pages            ' + "\n" +
' > helpQueries                           query help                 ' + "\n" +
' > exit                                                             ';

helpQueries = 
getHeadline("Select query help") +

'Create a select query:                                              ' + "\n" +
' > st = new AvocadoStatement(db, { "query" : "select..." });        ' + "\n" +
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
' > while (c.hasNext()) { print( c.next() ); }                       ';

helpAvocadoDatabase = 
getHeadline("AvocadoDatabase help") +
'AvocadoDatabase constructor:                                        ' + "\n" +
' > db2 = new AvocadoDatabase(connection);                           ' + "\n" +
'Functions:                                                          ' + "\n" +
'  _collections();                list all collections               ' + "\n" +
'                                 returns: list of AvocadoCollection ' + "\n" +
'  _collection(<name>);           get collection by name             ' + "\n" +
'                                 returns: AvocadoCollection         ' + "\n" +
'  _createStatement(<data>);      create and return select query     ' + "\n" +
'                                 returns: AvocadoStatement          ' + "\n" +
'  _help();                       this help                          ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  <collection names>                                                ';

helpAvocadoCollection = 
getHeadline("AvocadoCollection help") +
'AvocadoCollection constructor:                                      ' + "\n" +
' > col = db.mycoll;                                                 ' + "\n" +
'Functions:                                                          ' + "\n" +
'  save(<data>);                   create document and return id     ' + "\n" +
'  document(<id>);                 get document by id                ' + "\n" +
'  update(<id>, <new data>);       over writes document by id        ' + "\n" +
'  delete(<id>);                   deletes document by id            ' + "\n" +
'  _help();                        this help                         ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                       database object                   ' + "\n" +
'  _id                             collection id                     ' + "\n" +
'  name                            collection name                   ' + "\n" +
'  status                          status id                         ' + "\n" +
'  figures                                                           ';

helpAvocadoQueryCursor = 
getHeadline("AvocadoQueryCursor help") +
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
' > st = db._createStatement({ "query" : "select a from colA a" });  ' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > documents = c.elements();                                        ' + "\n" +
' > c = st.execute();                                                ' + "\n" +
' > while (c.hasNext()) { print( c.next() ); }                       ';

helpAvocadoStatement = 
getHeadline("AvocadoStatement help") +
'AvocadoStatement constructor:                                       ' + "\n" +
' > st = new AvocadoStatement({ "query" : "select..." });            ' + "\n" +
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
' > print(c.elements());                                             ';

helpExtended = 
getHeadline("More help") +
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
' > printPlain(x)                       print without pretty printing' + "\n" +
'                                       and without colors           ';

////////////////////////////////////////////////////////////////////////////////
/// @brief create the global db object and load the collections
////////////////////////////////////////////////////////////////////////////////

try {
  // default database
  db = new AvocadoDatabase(avocado);

  // load collection data
  db._collections();

  ModuleCache["/internal"].exports.db = db;

  print(HELP);
}
catch (err) {
  print(COLOR_RED + "connection failure: " + err + COLOR_BLACK);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
