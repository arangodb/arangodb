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
var helpAvocadoQueryInstance = "";
var helpAvocadoQueryTemplate = "";
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
  print(helpAvocadoQueryTemplate);
  print(helpAvocadoQueryInstance);
  print(helpAvocadoQueryCursor);
  print(helpExtended);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief log function
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"].exports.log = function(level, msg) {
  internal.output(level, ": ", msg, "\n");
}

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
  var str = this._database._connection.get("/_api/documents/" + encodeURIComponent(this.name));

  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
    
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["documents"];    
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single document from the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.document = function (id) {
  var str = this._database._connection.get("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["document"];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.save = function (data) {    
  var str = this._database._connection.post("/_api/document/" + encodeURIComponent(this.name), JSON.stringify(data));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["_id"];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.delete = function (id) {    
  var str = this._database._connection.delete("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  return !isErrorResult(requestResult);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.update = function (id, data) {    
  var str = this._database._connection.put("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id), JSON.stringify(data));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

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
  if (!this.data._id) {
    return false;
  }

  return this._hasNext;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next result document from the cursor
///
/// If no more results are available locally but more results are available on
/// the server, this function will make a roundtrip to the server
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryCursor.prototype.next = function () {
  if (!this.data._id) {
    throw "cursor has been disposed";
  }

  if (!this._hasNext) {
    throw "No more results";
  }
  
  var result = this.data.result[this._pos];
  this._pos++;
    
  if (this._pos == this._count) {
    // reached last result
    
    this._hasNext = false;
    this._pos = 0;
    
    if (this._hasMore) {
      this._hasMore = false;
      
      // load more results      
      var str = this._database._connection.put("/_api/cursor/"+ encodeURIComponent(this.data._id),  "");
      var requestResult = undefined;
      if (str != undefined) {
        requestResult = JSON.parse(str);
      }
    
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
  if (!this.data._id) {
    throw "cursor has been disposed";
  }

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
    throw "cursor has been disposed";
  }

  var str = this._database._connection.delete("/_api/cursor/"+ encodeURIComponent(this.data._id), "");
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
    
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
// --SECTION--                                              AvocadoQueryInstance
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoQueryInstance (database, data) {
  this._database = database;
  this.doCount = false;
  this.maxResults = null;
  this.bindVars = {};
  
  if (typeof data === "string") {
    this.query = data;
  }
  else if (data instanceof AvocadoQueryTemplate) {
    if (data.document.query == undefined) {
      data.load();
      if (data.document.query == undefined) {
        throw "could not load query string";
      }
    }
    this.query = data.document.query;
  }   
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bind a parameter to the query instance
///
/// This function can be called multiple times, once for each bind parameter.
/// All bind parameters will be transferred to the server in one go when 
/// execute() is called.
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryInstance.prototype.bind = function (key, value) {
  this.bindVars[key] = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the count flag for the query instance
///
/// Setting the count flag will make the query instance's cursor return the
/// total number of result documents. The count flag is not set by default.
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryInstance.prototype.setCount = function (bool) {
  this.doCount = bool ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the maximum number of results documents the cursor will return
/// in a single server roundtrip.
/// The higher this number is, the less server roundtrips will be made when
/// iterating over the result documents of a cursor.
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryInstance.prototype.setMax = function (value) {
  if (parseInt(value) > 0) {
    this.maxResults = parseInt(value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// Invoking execute() will transfer the query and all bind parameters to the
/// server. It will return a cursor with the query results in case of success.
/// In case of an error, the error will be printed
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryInstance.prototype.execute = function () {
  var body = {
    query : this.query,
    count : this.doCount,
    bindVars : this.bindVars
  }

  if (this.maxResults) {
    body["maxResults"] = this.maxResults;
  }
  
  var str = this._database._connection.post("/_api/cursor", JSON.stringify(body));
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
    
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return new AvocadoQueryCursor(this._database, requestResult);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoQueryInstance
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryInstance.prototype._help = function () {
  print(helpAvocadoQueryInstance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the query instance
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryInstance.prototype.toString = function () {  
  return getIdString(this, "AvocadoQueryInstance");
}

// -----------------------------------------------------------------------------
// --SECTION--                                              AvocadoQueryTemplate
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoQueryTemplate (database, data) {
  this._database = database;

  this.document = {
    collection : DEFAULT_QUERY_COLLECTION
  };
  
  if (typeof data === "string") {
    this.document.query = data;
  }
  else {    
    if (data["query"] != undefined) {
      this.document.query = data["query"];
    }

    if (data["_id"] != undefined) {
      this.document._id = data["_id"];
      this._id = this.document._id;
    }

    if (data["collection"] != undefined) {
      this.document.collection = data["collection"];
    }    

    if (data["name"] != undefined) {
      this.document.name = data["name"];
    }    

    // TODO additional attributes
  }
  
  if (this.document.query == undefined && this.document._id == undefined) {
    throw "AvocadoQueryTemplate needs query or _id.";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a new instance (AvocadoQueryInstance) for the query template
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryTemplate.prototype.getInstance = function () {
  if (this.document._id == undefined) {
    throw "no _id found";    
  }
  
  return new AvocadoQueryInstance(this._database, this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a query template, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryTemplate.prototype.update = function (data) {
  if (this.document._id == undefined) {
    throw "no _id found";    
  }
  
  if (typeof data === "string") {
    this.document.query = data;
  }
  else {
    // update query or name    
    if (data["query"] != undefined) {
      this.document.query = data["query"];
    }

    if (data["name"] != undefined) {
      this.document.name = data["name"];
    }    

    // TODO additional attributes
  }
  
  var queryCollection = new AvocadoCollection(this._database, this.document.collection);
  if (queryCollection.update(this.document._id, this.document)) {
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a query template, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryTemplate.prototype.delete = function () {
  if (this.document._id == undefined) {
    throw "no _id found";    
  }

  var queryCollection = new AvocadoCollection(this._database, this.document.collection);
  if (queryCollection.delete(this.document._id)) {
    this.document = {};
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief load a query template, identified by its id
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryTemplate.prototype.load = function () {
  if (this.document._id == undefined) {
    throw "AvocadoQueryTemplate needs _id for loading";    
  }

  var coll = this.document.collection;
  var queryCollection = new AvocadoCollection(this._database, coll);
  var requestResult = queryCollection.document(this.document._id);
    
  if (requestResult != undefined) {

    if (requestResult.query == undefined) {
      throw "document is not a query";    
    }
    // loaded
    this.document = requestResult;
    
    if (this.document.collection == undefined) {
      this.document.collection = coll;
    }
    
    return true;
  }

  return false;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a query template, and set its id in case of success
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryTemplate.prototype.save = function () {
  if (this.document._id != undefined) {
    throw "use update to save changes";    
  }
  
  var queryCollection = new AvocadoCollection(this._database, this.document.collection);
  var requestResult = queryCollection.save(this.document);
    
  if (requestResult != undefined) {
    // document saved
    this.document._id = requestResult;
    this._id = this.document._id;
    return true;
  }
  
  return false;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for AvocadoQueryTemplate
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryTemplate.prototype._help = function () {
  print(helpAvocadoQueryTemplate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the template
////////////////////////////////////////////////////////////////////////////////

AvocadoQueryTemplate.prototype.toString = function () {  
  return getIdString(this, "AvocadoQueryTemplate");
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
  var str = this._connection.get("/_api/collections");
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
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
  var str = this._connection.get("/_api/collection/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
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
/// @brief factory method to create a new query template
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._createQueryTemplate = function (queryData) {  
  var qt = new AvocadoQueryTemplate(this, queryData);
  if (qt.save()) {
    return qt;
  }
  
  return undefined;
}

AvocadoDatabase.prototype._getQueryTemplate = function (id) {  
  var qt = new AvocadoQueryTemplate(this, {"_id" : id});
  if (qt.load()) {
    return qt;
  }
  
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create a new query instance
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype._createQueryInstance = function (queryData) {  
  return new AvocadoQueryInstance(this, queryData);
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
getHeadline("Simple queries help") +
'Create query template:                                              ' + "\n" +
' > qt1 = db._createQueryTemplate("select ...");    simple query     ' + "\n" +
' > qt2 = db._createQueryTemplate(                  complex query    ' + "\n" +
'             {query:"select...",                                    ' + "\n" +
'              name:"qname",                                         ' + "\n" +
'              collection:"q"                                        ' + "\n" +
'              ... }                                                 ' + "\n" +
' > qt3 = db._getQueryTemplate("4334:2334");        query by id      ' + "\n" +
' > qt1.update("select ...");                       update           ' + "\n" +
' > qt1.delete("4334:2334");                        delete           ' + "\n" +
'Create query instance:                                              ' + "\n" +
' > qi1 = qt1.getInstance();                        from tmplate     ' + "\n" +
' > qi2 = db.createQueryInstance("select...");      without template ' + "\n" +
' > qi2.bind("key", "value");                                        ' + "\n" +
'Execute query:                                                      ' + "\n" +
' > cu1 = qi1.execute();                            returns cursor   ' + "\n" +
'Get all elements:                                                   ' + "\n" +
' > el = cu1.elements();                                             ' + "\n" +
'or loop over all results:                                           ' + "\n" +
' > while (cu1.hasNext()) { print( cu1.next() ); }                   ';

helpAvocadoDatabase = 
getHeadline("AvocadoDatabase help") +
'AvocadoDatabase constructor:                                        ' + "\n" +
' > db2 = new AvocadoDatabase(connection);                           ' + "\n" +
'Functions:                                                          ' + "\n" +
'  _collections();                list all collections               ' + "\n" +
'                                 returns: list of AvocadoCollection ' + "\n" +
'  _collection(<name>);           get collection by name             ' + "\n" +
'                                 returns: AvocadoCollection         ' + "\n" +
'  createQueryTemplate(<data>);   create and return query template   ' + "\n" +
'                                 returns: AvocadoQueryTemplate      ' + "\n" +
'  getQueryTemplate(<id>);        get query template by id           ' + "\n" +
'                                 returns: dvocadoQueryTemplate      ' + "\n" +
'  createQueryInstance(<data>);   create and return query instance   ' + "\n" +
'                                 returns: AvocadoQueryInstance      ' + "\n" +
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
' > qi2 = db.createQueryInstance("select a from colA a");            ' + "\n" +
' > cu1 = qi2.execute();                                             ' + "\n" +
' > documents = cu1.elements();                                      ' + "\n" +
' > cu1 = qi2.execute();                                             ' + "\n" +
' > while (cu1.hasNext()) { print( cu1.next() ); }                   ';

helpAvocadoQueryInstance = 
getHeadline("AvocadoQueryInstance help") +
'AvocadoQueryInstance constructor:                                   ' + "\n" +
' > qi1 = qt1.getInstance();                                         ' + "\n" +
' > qi2 = db._createQueryInstance("select ....");                    ' + "\n" +
'Functions:                                                          ' + "\n" +
'  bind(<key>, <value>);                 bind vars                   ' + "\n" +
'  setCount(true);                                                   ' + "\n" +
'  setMax(<max>);                                                    ' + "\n" +
'  execute();                            execute query and return    ' + "\n" +
'                                        query cursor                ' + "\n" +
'  _help();                              this help                   ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                             database object             ' + "\n" +
'  bindVars                              bind vars                   ' + "\n" +
'  doCount                               count results in server     ' + "\n" +
'  maxResults                            maximum number of results   ' + "\n" +
'  query                                 the query string            ' + "\n" +
'Example:                                                            ' + "\n" +
' > qi2 = db._createQueryInstance("select a from colA a              ' + "\n" +
'                             where a.x = @a@ and a.y = @b@");       ' + "\n" +
' > qi2.bind("a", "hello");                                          ' + "\n" +
' > qi2.bind("b", "world");                                          ' + "\n" +
' > cu1 = qi1.execute();                                             ';

helpAvocadoQueryTemplate = 
getHeadline("AvocadoQueryTemplate help") +
'AvocadoQueryTemplate constructor:                                   ' + "\n" +
' > qt1 = db._createQueryTemplate("select ...");    simple query     ' + "\n" +
' > qt2 = db._createQueryTemplate(                  complex query    ' + "\n" +
'             {query:"select...",                                    ' + "\n" +
'              name:"qname",                                         ' + "\n" +
'              collection:"q"                                        ' + "\n" +
'              ... }                                                 ' + "\n" +
'Functions:                                                          ' + "\n" +
'  update(<new data>);                  update query template        ' + "\n" +
'  delete(<id>);                        delete query template by id  ' + "\n" +
'  getInstance();                       get a query instance         ' + "\n" +
'                                       returns: AvocadoQueryInstance' + "\n" +
'  _help();                             this help                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                            database object              ' + "\n" +
'  _id                                  template id                  ' + "\n" +
'  name                                 collection name              ' + "\n" +
'Example:                                                            ' + "\n" +
' > qt1 = db._getQueryTemplate("4334:2334");                         ' + "\n" +
' > qt1.update("select a from collA a");                             ' + "\n" +
' > qi1 = qt1.getInstance();                                         ' + "\n" +
' > qt1.delete("4334:2334");                                         ';

helpExtended = 
getHeadline("More help") +
'Pager:                                                              ' + "\n" +
' > stop_pager()                        stop the pager output        ' + "\n" +
' > start_pager()                       start the pager              ' + "\n" +
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
