/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global require, arango, db, edges, ModuleCache, Module */

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       ArangoError
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
  if (error !== undefined) {
    this.error = error.error;
    this.code = error.code;
    this.errorNum = error.errorNum;
    this.errorMessage = error.errorMessage;
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

(function () {
  var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an error
////////////////////////////////////////////////////////////////////////////////

  ArangoError.prototype._PRINT = function() {
    internal.output(this.toString());
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief toString function
////////////////////////////////////////////////////////////////////////////////

  ArangoError.prototype.toString = function() {
    var result = "";

    if (typeof(internal.COLOR_BRIGHT) !== "undefined") {
      result = internal.COLOR_BRIGHT + "Error: " + internal.COLOR_OUTPUT_RESET;
    }
    else  {
      result = "Error: ";
    }

    result += "[" + this.code + ":" + this.errorNum + "] " + this.errorMessage;

    return result;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an error
////////////////////////////////////////////////////////////////////////////////

  ArangoError.prototype.toString = function() {
    var errorNum = this.errorNum;
    var errorMessage = this.errorMessage;

    return "[ArangoError " + errorNum + ": " + errorMessage + "]";
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "arangosh"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleArangosh
/// @{
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/arangosh"] = new Module("/arangosh");

(function () {
  var internal = require("internal");
  var client = ModuleCache["/arangosh"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief help texts
////////////////////////////////////////////////////////////////////////////////

  client.HELP = "";
  client.helpQueries = "";
  client.helpArangoDatabase = "";
  client.helpArangoCollection = "";
  client.helpArangoQueryCursor = "";
  client.helpArangoStatement = "";
  client.helpExtended = "";

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

  client.getIdString = function (object, typeName) {
    var result = "[object " + typeName;
  
    if (object._id) {
      result += ":" + object._id;
    }
    else if (object.data && object.data._id) {
      result += ":" + object.data._id;
    }

    result += "]";

    return result;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief handles error results
/// 
/// throws an exception in case of an an error
////////////////////////////////////////////////////////////////////////////////

  client.checkRequestResult = function (requestResult) {
    if (requestResult === undefined) {
      requestResult = {
        "error" : true,
        "code"  : 0,
        "errorNum" : 0,
        "errorMessage" : "Unknown error. Request result is empty"
      };
    }
  
    if (requestResult.error !== undefined && requestResult.error) {    
      throw new ArangoError(requestResult);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief create a formatted headline text 
////////////////////////////////////////////////////////////////////////////////

  client.createHelpHeadline = function (text) {
    var i;
    var p = "";
    var x = parseInt(Math.abs(78 - text.length) / 2);

    for (i = 0; i < x; ++i) {
      p += "-";
    }
  
    return "\n" + p + " " + text + " " + p + "\n";
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief turn off pretty printing
////////////////////////////////////////////////////////////////////////////////

function print_plain (data) {
  var internal = require("internal");
  var p = internal.PRETTY_PRINT;
  internal.PRETTY_PRINT = false;

  var c = undefined;

  if (typeof(internal.COLOR_OUTPUT) !== "undefined") {
    c = internal.COLOR_OUTPUT;
    internal.COLOR_OUTPUT = undefined;
  }
  
  try {
    internal.print(data);

    internal.PRETTY_PRINT = p;
    internal.COLOR_OUTPUT = c;
  }
  catch (e) {
    internal.PRETTY_PRINT = p;
    internal.COLOR_OUTPUT = c;

    throw e.message;    
  }  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing
////////////////////////////////////////////////////////////////////////////////

function start_pretty_print () {
  var internal = require("internal");

  if (! internal.PRETTY_PRINT) {
    internal.print("using pretty printing");
    internal.PRETTY_PRINT = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_pretty_print () {
  var internal = require("internal");

  if (internal.PRETTY_PRINT) {
    internal.PRETTY_PRINT = false;
    internal.print("stop pretty printing");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start paging
////////////////////////////////////////////////////////////////////////////////

function start_pager () {
  var internal = require("internal");

  internal.start_pager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop paging
////////////////////////////////////////////////////////////////////////////////

function stop_pager () {
  var internal = require("internal");
  
  internal.stop_pager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing with optional color
////////////////////////////////////////////////////////////////////////////////

function start_color_print (color) {
  var internal = require("internal");

  if (typeof(color) === "string") {
    internal.COLOR_OUTPUT = color;
  }
  else {
    internal.COLOR_OUTPUT = internal.COLOR_BRIGHT;
  }

  internal.print("start "
                 + internal.COLOR_OUTPUT 
                 + "color" 
                 + internal.COLOR_OUTPUT_RESET
                 + " printing");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_color_print () {
  var internal = require("internal");

  internal.print("stop color printing");
  internal.COLOR_OUTPUT = undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the overall help
////////////////////////////////////////////////////////////////////////////////

function help () {
  var internal = require("internal");
  var client = require("arangosh");

  internal.print(client.HELP);
  internal.print(client.helpQueries);
  internal.print(client.helpArangoDatabase);
  internal.print(client.helpArangoCollection);
  internal.print(client.helpArangoStatement);
  internal.print(client.helpArangoQueryCursor);
  internal.print(client.helpExtended);
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

(function () {
  var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief be quiet
////////////////////////////////////////////////////////////////////////////////

  internal.ARANGO_QUIET = ARANGO_QUIET;

////////////////////////////////////////////////////////////////////////////////
/// @brief log function
////////////////////////////////////////////////////////////////////////////////

  internal.log = function (level, msg) {
    internal.output(level, ": ", msg, "\n");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the routing cache
////////////////////////////////////////////////////////////////////////////////

  internal.reloadRouting = function () {
    if (typeof arango !== 'undefined') {
      arango.POST("/_admin/reloadRouting", "");
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                 ArangoQueryCursor
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
  
  if (data.result !== undefined) {
    this._count = data.result.length;
    
    if (this._pos < this._count) {
      this._hasNext = true;
    }
    
    if (data.hasMore !== undefined && data.hasMore) {
      this._hasMore = true;
    }    
  }
}

(function () {
  var internal = require("internal");
  var client = require("arangosh");

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

  client.helpArangoQueryCursor = client.createHelpHeadline("ArangoQueryCursor help") +
  'ArangoQueryCursor constructor:                                      ' + "\n" +
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

  ArangoQueryCursor.prototype._help = function () {
    internal.print(client.helpArangoQueryCursor);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the cursor
////////////////////////////////////////////////////////////////////////////////

  ArangoQueryCursor.prototype.toString = function () {  
    return client.getIdString(this, "ArangoQueryCursor");
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
/// @brief return whether there are more results available in the cursor
////////////////////////////////////////////////////////////////////////////////

  ArangoQueryCursor.prototype.hasNext = function () {
    return this._hasNext;
  };

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
    if (this._pos === this._count) {
      this._hasNext = false;
      this._pos = 0;

      if (this._hasMore && this.data.id) {
        this._hasMore = false;

        // load more results      
        var requestResult = this._database._connection.PUT(
          "/_api/cursor/"+ encodeURIComponent(this.data.id),
          "");

        client.checkRequestResult(requestResult);

        this.data = requestResult;
        this._count = requestResult.result.length;

        if (this._pos < this._count) {
          this._hasNext = true;
        }

        if (requestResult.hasMore !== undefined && requestResult.hasMore) {
          this._hasMore = true;
        }                
      }    
    }

    return result;
  };

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
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly dispose the cursor
///
/// Calling this function will mark the cursor as deleted on the server. It will
/// therefore make a roundtrip to the server. Using a cursor after it has been
/// disposed is considered a user error
////////////////////////////////////////////////////////////////////////////////

  ArangoQueryCursor.prototype.dispose = function () {
    if (!this.data.id) {
      // client side only cursor
      return;
    }

    var requestResult = this._database._connection.DELETE(
      "/_api/cursor/"+ encodeURIComponent(this.data.id),
      "");

    client.checkRequestResult(requestResult);

    this.data.id = undefined;
  };

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
    if (!this.data.id) {
      throw "cursor has been disposed";
    }

    return this.data.count;
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoStatement
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
    
  if (data.query === undefined || data.query === "") {
    throw "ArangoStatement needs a valid query attribute";
  }
  this.setQuery(data.query);

  if (data.count !== undefined) {
    this.setCount(data.count);
  }
  if (data.batchSize !== undefined) {
    this.setBatchSize(data.batchSize);
  }
}

(function () {
  var internal = require("internal");
  var client = require("arangosh");

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

  client.helpArangoStatement = client.createHelpHeadline("ArangoStatement help") +
  'ArangoStatement constructor:                                        ' + "\n" +
  ' > st = new ArangoStatement(db, { "query" : "for ..." });           ' + "\n" +
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
  '                              c.x == @a && c.y == @b return c" });  ' + "\n" +
  ' > st.bind("a", "hello");                                           ' + "\n" +
  ' > st.bind("b", "world");                                           ' + "\n" +
  ' > c = st.execute();                                                ' + "\n" +
  ' > print(c.elements());                                             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for ArangoStatement
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype._help = function () {
    internal.print(client.helpArangoStatement);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the statement
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.toString = function () {  
    return client.getIdString(this, "ArangoStatement");
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
/// @brief bind a parameter to the statement
///
/// This function can be called multiple times, once for each bind parameter.
/// All bind parameters will be transferred to the server in one go when 
/// execute() is called.
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.bind = function (key, value) {
    if (key instanceof Object) {
      if (value !== undefined) {
        throw "invalid bind parameter declaration";
      }

      this._bindVars = key;
    }
    else if (typeof(key) === "string") {
      if (this._bindVars[key] !== undefined) {
        throw "redeclaration of bind parameter";
      }

      this._bindVars[key] = value;
    }
    else if (typeof(key) === "number") {
      var strKey = String(parseInt(key));

      if (strKey !== String(key)) {
        throw "invalid bind parameter declaration";
      }

      if (this._bindVars[strKey] !== undefined) {
        throw "redeclaration of bind parameter";
      }

      this._bindVars[strKey] = value;
    }
    else {
      throw "invalid bind parameter declaration";
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief return the bind variables already set
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.getBindVariables = function () {
    return this._bindVars;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief get the count flag for the statement
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.getCount = function () {
    return this._doCount;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief get the maximum number of results documents the cursor will return
/// in a single server roundtrip.
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.getBatchSize = function () {
    return this._batchSize;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief get query string
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.getQuery = function () {
    return this._query;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief set the count flag for the statement
///
/// Setting the count flag will make the statement's result cursor return the
/// total number of result documents. The count flag is not set by default.
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.setCount = function (bool) {
    this._doCount = bool ? true : false;
  };

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
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query string
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.setQuery = function (query) {
    this._query = query;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return the results
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.parse = function () {
    var body = {
      "query" : this._query,
    };

    var requestResult = this._database._connection.POST(
      "/_api/query",
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return true;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// Invoking execute() will transfer the query and all bind parameters to the
/// server. It will return a cursor with the query results in case of success.
/// In case of an error, the error will be printed
////////////////////////////////////////////////////////////////////////////////

  ArangoStatement.prototype.execute = function () {
    var body = {
      "query" : this._query,
      "count" : this._doCount,
      "bindVars" : this._bindVars
    };

    if (this._batchSize) {
      body["batchSize"] = this._batchSize;
    }

    var requestResult = this._database._connection.POST(
      "/_api/cursor",
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return new ArangoQueryCursor(this._database, requestResult);
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

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

function ArangoCollection (database, data) {
  this._database = database;

  if (typeof data === "string") {
    this._id = null;
    this._name = data;
    this._status = null;
  }
  else if (data !== undefined) {
    this._id = data.id;
    this._name = data.name;
    this._status = data.status;
  }
  else {
    this._id = null;
    this._name = null;
    this._status = null;
  }
}

(function () {
  var internal = require("internal");
  var client = require("arangosh");

  internal.ArangoCollection = ArangoCollection;

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

  client.helpArangoCollection = client.createHelpHeadline("ArangoCollection help") +
  'ArangoCollection constructor:                                       ' + "\n" +
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
    return client.getIdString(this, "ArangoCollection");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the collection
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

    internal.output("[ArangoCollection ",
                    this._id, 
                    ", \"", 
                    this.name(), 
                    "\" (status ",
                    status,
                    ")]");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype._help = function () {  
    internal.print(client.helpArangoCollection);
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
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.name = function () {
    if (this._name === null) {
      this.refresh();
    }

    return this._name;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.status = function () {
    var result;

    if (this._status === null) {
      this.refresh();
    }

    // save original status
    result = this._status;

    if (this._status == ArangoCollection.STATUS_UNLOADING) {
      // if collection is currently unloading, we must not cache this info
      this._status = null;
    }

    // return the correct result
    return result;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.properties = function (properties) {
    var requestResult;

    if (properties === undefined) {
      requestResult = this._database._connection.GET(
        "/_api/collection/" + encodeURIComponent(this._id) + "/properties");

      client.checkRequestResult(requestResult);
    }
    else {
      var body = {};

      if (properties.hasOwnProperty("waitForSync")) {
        body.waitForSync = properties.waitForSync;
      }

      requestResult = this._database._connection.PUT(
        "/_api/collection/" + encodeURIComponent(this._id) + "/properties",
        JSON.stringify(body));

      client.checkRequestResult(requestResult);
    }

    return { 
      waitForSync : requestResult.waitForSync,
      journalSize : requestResult.journalSize
    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the figures of a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.figures = function () {
    var requestResult = this._database._connection.GET(
      "/_api/collection/" + encodeURIComponent(this._id) + "/figures");

    client.checkRequestResult(requestResult);

    return requestResult.figures;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.drop = function () {
    var requestResult = this._database._connection.DELETE(
      "/_api/collection/" + encodeURIComponent(this._id));

    client.checkRequestResult(requestResult);

    this._status = ArangoCollection.STATUS_DELETED;

    var database = this._database;
    var name;

    for (name in database) {
      if (database.hasOwnProperty(name)) {
        var collection = database[name];

        if (collection instanceof ArangoCollection) {
          if (collection._id === this._id) {
            delete database[name];
          }
        }
      }
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all documents
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.toArray = function () {
    return this.all().toArray();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all indexes
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.getIndexes = function () {
    var requestResult = this._database._connection.GET(
      "/_api/index?collection=" + encodeURIComponent(this._id));

    client.checkRequestResult(requestResult);

    return requestResult.indexes;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns one index
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.index = function (id) {
    if (id.hasOwnProperty("id")) {
      id = id.id;
    }

    var requestResult;
    var s = id.split("/");

    if (s.length !== 2) {
      requestResult = {
        errorNum: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code,
        errorMessage: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message
      };

      throw new ArangoError(requestResult);
    }
    else if (parseInt(s[0]) !== this._id) {
      requestResult = {
        errorNum: internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code,
        errorMessage: 
          internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.message
      };

      throw new ArangoError(requestResult);
    }

    requestResult = this._database._connection.GET("/_api/index/" + id);

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes one index
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.dropIndex = function (id) {
    if (id.hasOwnProperty("id")) {
      id = id.id;
    }

    var requestResult;
    var s = id.split("/");

    if (s.length !== 2) {
      requestResult = {
        errorNum: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code,
        errorMessage: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message
      };

      throw new ArangoError(requestResult);
    }
    else if (parseInt(s[0]) !== this._id) {
      requestResult = {
        errorNum: internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code,
        errorMessage:
          internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.message
      };

      throw new ArangoError(requestResult);
    }

    requestResult = this._database._connection.DELETE("/_api/index/" + id);

    if (requestResult !== null
        && requestResult.error === true 
        && requestResult.errorNum
             === internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code) {
      return false;
    }

    client.checkRequestResult(requestResult);

    return true;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a cap constraint
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.ensureCapConstraint = function (size) {
    var body;

    body = { type : "cap", size : size };

    var requestResult = this._database._connection.POST(
      "/_api/index?collection=" + encodeURIComponent(this._id),
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a unique skip-list index
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.ensureUniqueSkiplist = function () {
    var i;
    var body;
    var fields = [];

    for (i = 0;  i < arguments.length;  ++i) {
      fields.push(arguments[i]);
    }

    body = { type : "skiplist", unique : true, fields : fields };

    var requestResult = this._database._connection.POST(
      "/_api/index?collection=" + encodeURIComponent(this._id),
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a skip-list index
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.ensureSkiplist = function () {
    var i;
    var body;
    var fields = [];

    for (i = 0;  i < arguments.length;  ++i) {
      fields.push(arguments[i]);
    }

    body = { type : "skiplist", unique : false, fields : fields };

    var requestResult = this._database._connection.POST(
      "/_api/index?collection=" + encodeURIComponent(this._id),
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a unique constraint
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.ensureUniqueConstraint = function () {
    var i;
    var body;
    var fields = [];

    for (i = 0;  i < arguments.length;  ++i) {
      fields.push(arguments[i]);
    }

    body = { type : "hash", unique : true, fields : fields };

    var requestResult = this._database._connection.POST(
      "/_api/index?collection=" + encodeURIComponent(this._id),
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a hash index
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.ensureHashIndex = function () {
    var i;
    var body;
    var fields = [];

    for (i = 0;  i < arguments.length;  ++i) {
      fields.push(arguments[i]);
    }

    body = { type : "hash", unique : false, fields : fields };

    var requestResult = this._database._connection.POST(
      "/_api/index?collection=" + encodeURIComponent(this._id),
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an geo index
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.ensureGeoIndex = function (lat, lon) {
    var body;

    if (typeof lat !== "string") {
      throw "usage: ensureGeoIndex(<lat>, <lon>) or ensureGeoIndex(<loc>[, <geoJson>])";
    }

    if (typeof lon === "boolean") {
      body = { 
        type : "geo", 
        fields : [ lat ], 
        geoJson : lon, 
        constraint : false
      };
    }
    else if (lon === undefined) {
      body = {
        type : "geo", 
        fields : [ lat ],
        geoJson : false, 
        constraint : false 
      };
    }
    else {
      body = {
        type : "geo",
        fields : [ lat, lon ],
        constraint : false
      };
    }

    var requestResult = this._database._connection.POST(
      "/_api/index?collection=" + encodeURIComponent(this._id), 
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an geo constraint
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.ensureGeoConstraint = function (lat, lon, ignoreNull) {
    var body;

    // only two parameter
    if (ignoreNull === undefined) {
      ignoreNull = lon;

      if (typeof ignoreNull !== "boolean") {
        throw "usage: ensureGeoConstraint(<lat>, <lon>, <ignore-null>)"
            + " or ensureGeoConstraint(<lat>, <geo-json>, <ignore-null>)";
      }

      body = {
        type : "geo",
        fields : [ lat ],
        geoJson : false, 
        constraint : true,
        ignoreNull : ignoreNull 
      };
    }

    // three parameter
    else {
      if (typeof lon === "boolean") {
        body = {
          type : "geo",
          fields : [ lat ],
          geoJson : lon,
          constraint : true,
          ignoreNull : ignoreNull
        };
      }
      else {
        body = {
          type : "geo",
          fields : [ lat, lon ],
          constraint : true,
          ignoreNull : ignoreNull
        };
      }
    }

    var requestResult = this._database._connection.POST(
      "/_api/index?collection=" + encodeURIComponent(this._id),
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief queries by example
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.BY_EXAMPLE_HASH = function (index, example, skip, limit) {
    var key;
    var body;

    limit = limit || null;
    skip = skip || null;

    if (index.hasOwnProperty("id")) {
      index = index.id;
    }

    body = {
      collection : this._id,
      index : index,
      skip : skip,
      limit : limit,
      example : {} 
    };

    for (key in example) {
      if (example.hasOwnProperty(key)) {
        body.example[key] = example[key];
      }
    }

    var requestResult = this._database._connection.PUT(
      "/_api/simple/BY-EXAMPLE-HASH",
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.truncate = function () {
    var requestResult = this._database._connection.PUT(
      "/_api/collection/" + encodeURIComponent(this._id) + "/truncate", "");

    client.checkRequestResult(requestResult);

    this._status = null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.load = function () {
    var requestResult = this._database._connection.PUT(
      "/_api/collection/" + encodeURIComponent(this._id) + "/load",
      "");

    client.checkRequestResult(requestResult);

    this._status = null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.unload = function () {
    var requestResult = this._database._connection.PUT(
      "/_api/collection/" + encodeURIComponent(this._id) + "/unload",
      "");

    client.checkRequestResult(requestResult);

    this._status = null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.rename = function (name) {
    var body = { name : name };
    var requestResult = this._database._connection.PUT(
      "/_api/collection/" + encodeURIComponent(this._id) + "/rename",
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    delete this._database[this._name];
    this._database[name] = this;

    this._status = null;
    this._name = null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief refreshes a collection status and name
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.refresh = function () {
    var requestResult = this._database._connection.GET(
      "/_api/collection/" + encodeURIComponent(this._id));

    client.checkRequestResult(requestResult);

    this._name = requestResult['name'];
    this._status = requestResult['status'];
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
/// @brief returns the number of documents
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.count = function () {
    var requestResult = this._database._connection.GET(
      "/_api/collection/" + encodeURIComponent(this._id) + "/count");

    client.checkRequestResult(requestResult);

    return requestResult["count"];
  };

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

    if (rev === null) {
      requestResult = this._database._connection.GET("/_api/document/" + id);
    }
    else {
      requestResult = this._database._connection.GET(
        "/_api/document/" + id,
        {'if-match' : '"' + rev + '"' });
    }

    if (requestResult !== null
        && requestResult.error === true 
        && requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
      var s = id.split("/");

      if (s.length !== 2) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      }
      else if (parseInt(s[0]) !== this._id) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code;
      }

      throw new ArangoError(requestResult);
    }

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.save = function (data) {    
    var requestResult = this._database._connection.POST(
      "/_api/document?collection=" + encodeURIComponent(this._id),
      JSON.stringify(data));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

  ArangoCollection.prototype.remove = function (id, overwrite) {
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

    if (rev === null) {
      requestResult = this._database._connection.DELETE(
        "/_api/document/" + id + policy);
    }
    else {
      requestResult = this._database._connection.DELETE(
        "/_api/document/" + id + policy, {'if-match' : '"' + rev + '"' });
    }

    if (requestResult !== null && requestResult.error === true) {
      var s = id.split("/");

      if (s.length !== 2) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      }
      else if (parseInt(s[0]) !== this._id) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code;
      }

      if (overwrite) {
        if (requestResult.errorNum === internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
          return false;
        }
      }

      throw new ArangoError(requestResult);
    }

    client.checkRequestResult(requestResult);

    return true;
  };

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

    if (rev === null) {
      requestResult = this._database._connection.PUT(
        "/_api/document/" + id + policy, 
        JSON.stringify(data));
    }
    else {
      requestResult = this._database._connection.PUT(
        "/_api/document/" + id + policy, JSON.stringify(data),
        {'if-match' : '"' + rev + '"' });
    }

    if (requestResult !== null && requestResult.error === true) {
      var s = id.split("/");

      if (s.length !== 2) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      }
      else if (parseInt(s[0]) !== this._id) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code;
      }

      throw new ArangoError(requestResult);
    }

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

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

function ArangoDatabase (connection) {
  this._connection = connection;
  this._collectionConstructor = ArangoCollection;
}

(function () {
  var internal = require("internal");
  var client = require("arangosh");

  internal.ArangoDatabase = ArangoDatabase;

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

  client.helpArangoDatabase = client.createHelpHeadline("ArangoDatabase help") +
  'ArangoDatabase constructor:                                         ' + "\n" +
  ' > db = new ArangoDatabase(connection);                             ' + "\n" +
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
/// @brief print the help for ArangoDatabase
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._help = function () {  
    internal.print(client.helpArangoDatabase);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the database object
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype.toString = function () {  
    return "[object ArangoDatabase]";
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
/// @brief return all collections from the database
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._collections = function () {
    var requestResult = this._connection.GET("/_api/collection");
  
    client.checkRequestResult(requestResult);

    if (requestResult["collections"] !== undefined) {
      var collections = requestResult["collections"];
      var result = [];
      var i;
    
      // add all collentions to object
      for (i = 0;  i < collections.length;  ++i) {
        var collection = new this._collectionConstructor(this, collections[i]);

        this[collection._name] = collection;
        result.push(collection);
      }
      
      return result;
    }
  
    return undefined;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single collection, identified by its id or name
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._collection = function (id) {
    var requestResult = this._connection.GET(
      "/_api/collection/" + encodeURIComponent(id));
  
    // return null in case of not found
    if (requestResult !== null
        && requestResult.error === true 
        && requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
      return null;
    }

    // check all other errors and throw them
    client.checkRequestResult(requestResult);

    var name = requestResult["name"];

    if (name !== undefined) {
      this[name] = new this._collectionConstructor(this, requestResult);
      return this[name];
    }
  
    return undefined;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._create = function (name, properties) {
    var body = {
      "name" : name
    };

    if (properties !== undefined) {
      if (properties.hasOwnProperty("waitForSync")) {
        body.waitForSync = properties.waitForSync;
      }

      if (properties.hasOwnProperty("journalSize")) {
        body.journalSize = properties.journalSize;
      }

      if (properties.hasOwnProperty("isSystem")) {
        body.isSystem = properties.isSystem;
      }
    }

    var requestResult = this._connection.POST(
      "/_api/collection",
      JSON.stringify(body));

    client.checkRequestResult(requestResult);

    var nname = requestResult["name"];

    if (nname !== undefined) {
      this[nname] = new this._collectionConstructor(this, requestResult);
      return this[nname];
    }
  
    return undefined;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._truncate = function (id) {
    var name;

    for (name in this) {
      if (this.hasOwnProperty(name)) {
        var collection = this[name];

        if (collection instanceof this._collectionConstructor) {
          if (collection._id === id || collection._name === id) {
            return collection.truncate();
          }
        }
      }
    }

    return undefined;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._drop = function (id) {
    var name;

    for (name in this) {
      if (this.hasOwnProperty(name)) {
        var collection = this[name];

        if (collection instanceof this._collectionConstructor) {
          if (collection._id === id || collection._name === id) {
            return collection.drop();
          }
        }
      }
    }

    return undefined;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns one index
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._index = function (id) {
    if (id.hasOwnProperty("id")) {
      id = id.id;
    }

    var requestResult = this._connection.GET("/_api/index/" + id);

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes one index
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._dropIndex = function (id) {
    if (id.hasOwnProperty("id")) {
      id = id.id;
    }

    var requestResult = this._connection.DELETE("/_api/index/" + id);

    if (requestResult !== null
        && requestResult.error === true 
        && requestResult.errorNum === internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code) {
      return false;
    }

    client.checkRequestResult(requestResult);

    return true;
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

    if (rev === null) {
      requestResult = this._connection.GET("/_api/document/" + id);
    }
    else {
      requestResult = this._connection.GET(
        "/_api/document/" + id,
        {'if-match' : '"' + rev + '"' });
    }

    if (requestResult !== null
        && requestResult.error === true 
        && requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
      var s = id.split("/");

      if (s.length !== 2) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      }

      throw new ArangoError(requestResult);
    }

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._remove = function (id, overwrite) {
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

    if (rev === null) {
      requestResult = this._connection.DELETE("/_api/document/" + id + policy);
    }
    else {
      requestResult = this._connection.DELETE(
        "/_api/document/" + id + policy,
        {'if-match' : '"' + rev + '"' });
    }

    if (requestResult !== null && requestResult.error === true) {
      var s = id.split("/");

      if (s.length !== 2) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      }

      if (overwrite) {
        if (requestResult.errorNum === internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
          return false;
        }
      }

      throw new ArangoError(requestResult);
    }

    client.checkRequestResult(requestResult);

    return true;
  };

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

    if (rev === null) {
      requestResult = this._connection.PUT(
        "/_api/document/" + id + policy,
        JSON.stringify(data));
    }
    else {
      requestResult = this._connection.PUT(
        "/_api/document/" + id + policy,
        JSON.stringify(data),
        {'if-match' : '"' + rev + '"' });
    }

    if (requestResult !== null && requestResult.error === true) {
      var s = id.split("/");

      if (s.length !== 2) {
        requestResult.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      }

      throw new ArangoError(requestResult);
    }

    client.checkRequestResult(requestResult);

    return requestResult;
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
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                             ArangoEdgesCollection
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

(function () {
  var internal = require("internal");
  var client = require("arangosh");

  internal.ArangoEdgesCollection = ArangoEdgesCollection;

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

  client.helpArangoEdgesCollection = client.createHelpHeadline("ArangoEdgesCollection help") +
  'ArangoEdgesCollection constructor:                                  ' + "\n" +
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
    return client.getIdString(this, "ArangoEdgesCollection");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the collection
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

    internal.output("[ArangoEdgesCollection ",
                    this._id,
                    ", \"",
                    this.name(),
                    "\" (status " + status + ")]");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

  ArangoEdgesCollection.prototype._help = function () {  
    internal.print(client.helpArangoEdgesCollection);
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
/// @brief save a document in the collection, return its id
////////////////////////////////////////////////////////////////////////////////

  ArangoEdgesCollection.prototype.save = function (from, to, data) {    
    if (from.hasOwnProperty("_id")) {
      from = from._id;
    }

    if (to.hasOwnProperty("_id")) {
      to = to._id;
    }

    var url = "/_api/edge?collection=" + encodeURIComponent(this._id)
            + "&from=" + encodeURIComponent(from)
            + "&to=" + encodeURIComponent(to);

    var requestResult = this._database._connection.POST(
      url,
      JSON.stringify(data));

    client.checkRequestResult(requestResult);

    return requestResult;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting or ending in a vertex
////////////////////////////////////////////////////////////////////////////////

  ArangoEdgesCollection.prototype.edges = function (vertex) {

    // if vertex is a list, iterator and concat
    if (vertex instanceof Array) {
      var edges = [];
      var i;

      for (i = 0;  i < vertex.length;  ++i) {
        var e = this.edges(vertex[i]);

        edges.push.apply(edges, e);
      }

      return edges;
    }

    if (vertex.hasOwnProperty("_id")) {
      vertex = vertex._id;
    }

    // get the edges
    var requestResult = this._database._connection.GET(
      "/_api/edges/" + encodeURIComponent(this._id) + "?vertex=" + encodeURIComponent(vertex));

    client.checkRequestResult(requestResult);

    return requestResult['edges'];
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges ending in a vertex
////////////////////////////////////////////////////////////////////////////////

  ArangoEdgesCollection.prototype.inEdges = function (vertex) {

    // if vertex is a list, iterator and concat
    if (vertex instanceof Array) {
      var edges = [];
      var i;

      for (i = 0;  i < vertex.length;  ++i) {
        var e = this.inEdges(vertex[i]);

        edges.push.apply(edges, e);
      }

      return edges;
    }

    if (vertex.hasOwnProperty("_id")) {
      vertex = vertex._id;
    }

    // get the edges
    var requestResult = this._database._connection.GET(
      "/_api/edges/" + encodeURIComponent(this._id)
      + "?direction=in&vertex=" + encodeURIComponent(vertex));

    client.checkRequestResult(requestResult);

    return requestResult['edges'];
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting in a vertex
////////////////////////////////////////////////////////////////////////////////

  ArangoEdgesCollection.prototype.outEdges = function (vertex) {

    // if vertex is a list, iterator and concat
    if (vertex instanceof Array) {
      var edges = [];
      var i;

      for (i = 0;  i < vertex.length;  ++i) {
        var e = this.outEdges(vertex[i]);

        edges.push.apply(edges, e);
      }

      return edges;
    }

    if (vertex.hasOwnProperty("_id")) {
      vertex = vertex._id;
    }

    // get the edges
    var requestResult = this._database._connection.GET(
      "/_api/edges/" + encodeURIComponent(this._id)
      + "?direction=out&vertex=" + encodeURIComponent(vertex));

    client.checkRequestResult(requestResult);

    return requestResult['edges'];
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       ArangoEdges
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

(function () {
  var internal = require("internal");
  var client = require("arangosh");

  internal.ArangoEdges = ArangoEdges;

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

  client.helpArangoEdges = client.createHelpHeadline("ArangoEdges help") +
  'ArangoEdges constructor:                                            ' + "\n" +
  ' > edges = new ArangoEdges(connection);                             ' + "\n" +
  '                                                                    ' + "\n" +
  'Administration Functions:                                           ' + "\n" +
  '  _help();                       this help                          ' + "\n" +
  '                                                                    ' + "\n" +
  'Attributes:                                                         ' + "\n" +
  '  <collection names>             collection with the given name     ';

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for ArangoEdges
////////////////////////////////////////////////////////////////////////////////

  ArangoEdges.prototype._help = function () {  
    internal.print(client.helpArangoEdges);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the database object
////////////////////////////////////////////////////////////////////////////////

  ArangoEdges.prototype.toString = function () {  
    return "[object ArangoEdges]";
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                      initialisers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");
  var client = require("arangosh");

////////////////////////////////////////////////////////////////////////////////
/// @brief general help
////////////////////////////////////////////////////////////////////////////////

  client.HELP = client.createHelpHeadline("Help") +
  'Predefined objects:                                                 ' + "\n" +
  '  arango:                                ArangoConnection           ' + "\n" +
  '  db:                                    ArangoDatabase             ' + "\n" +
  '  edges:                                 ArangoEdges                ' + "\n" +
  'Example:                                                            ' + "\n" +
  ' > db._collections();                    list all collections       ' + "\n" +
  ' > db.<coll_name>.all().toArray();       list all documents         ' + "\n" +
  ' > id = db.<coll_name>.save({ ... });    save a document            ' + "\n" +
  ' > db.<coll_name>.remove(<_id>);         delete a document          ' + "\n" +
  ' > db.<coll_name>.document(<_id>);       get a document             ' + "\n" +
  ' > help                                  show help pages            ' + "\n" +
  ' > exit                                                             ';

////////////////////////////////////////////////////////////////////////////////
/// @brief query help
////////////////////////////////////////////////////////////////////////////////

  client.helpQueries = client.createHelpHeadline("Select query help") +
  'Create a select query:                                              ' + "\n" +
  ' > st = new ArangoStatement(db, { "query" : "for..." });            ' + "\n" +
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

  client.helpExtended = client.createHelpHeadline("More help") +
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
  ' > print_plain(x)                      print without pretty printing' + "\n" +
  '                                       and without colors           ';

////////////////////////////////////////////////////////////////////////////////
/// @brief create the global db object and load the collections
////////////////////////////////////////////////////////////////////////////////

  try {
    if (typeof arango !== 'undefined') {

      // default databases
      db = internal.db = new ArangoDatabase(arango);
      edges = internal.edges = new ArangoEdges(arango);

      // load collection data
      internal.db._collections();
      internal.edges._collections();

      // load simple queries
      require("simple-query");

      if (! internal.ARANGO_QUIET) {
        internal.print(client.HELP);
      }
    }
  }
  catch (err) {
    internal.print(String(err));
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint\\)"
// End:
