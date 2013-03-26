/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global $, jqconsole */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB web browser shell
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012-2013 triagens GmbH, Cologne, Germany
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

// -----------------------------------------------------------------------------
// --SECTION--                                                            Module
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief module constructor
////////////////////////////////////////////////////////////////////////////////

function Module (id) {
  this.id = id;
  this.exports = {};
  this.definition = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief module cache
////////////////////////////////////////////////////////////////////////////////

Module.prototype.ModuleCache = {};
Module.prototype.ModuleCache["/internal"] = new Module("/internal");

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global module
////////////////////////////////////////////////////////////////////////////////

var module = Module.prototype.ModuleCache["/"] = new Module("/");

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalises a path
////////////////////////////////////////////////////////////////////////////////

Module.prototype.normalise = function (path) {
  var i;
  var n;
  var p;
  var q;
  var x;

  if (path === "") {
    return this.id;
  }

  p = path.split('/');

  // relative path
  if (p[0] === "." || p[0] === "..") {
    q = this.id.split('/');
    q.pop();
    q = q.concat(p);
  }

  // absolute path
  else {
    q = p;
  }

  // normalize path
  n = [];

  for (i = 0;  i < q.length;  ++i) {
    x = q[i];

    if (x === "..") {
      if (n.length === 0) {
        throw "cannot cross module top";
      }

      n.pop();
    }
    else if (x !== "" && x !== ".") {
      n.push(x);
    }
  }

  return "/" + n.join('/');
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a new module definition
////////////////////////////////////////////////////////////////////////////////

Module.prototype.define = function (path, definition) {

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (! Module.prototype.ModuleCache.hasOwnProperty(path)) {
    Module.prototype.ModuleCache[path] = new Module(path);
  }

  Module.prototype.ModuleCache[path].definition = definition;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a definition or creates a new module descriptor
////////////////////////////////////////////////////////////////////////////////

Module.prototype.require = function (path) {
  var module;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (Module.prototype.ModuleCache.hasOwnProperty(path)) {
    module = Module.prototype.ModuleCache[path];
  }
  else {
    module = Module.prototype.ModuleCache[path] = new Module(path);
  }

  if (module.definition !== null) {
    var definition;

    definition = module.definition;
    module.definition = null;
    definition(module.exports, module);
  }

  return module.exports;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief global require function
////////////////////////////////////////////////////////////////////////////////

function require (path) {
  return module.require(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief global print function
////////////////////////////////////////////////////////////////////////////////

function print () {
  var internal = require("internal");

  internal.print.apply(internal.print, arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoConnection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief arango server connection
////////////////////////////////////////////////////////////////////////////////

function ArangoConnection () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a get request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.get = function (url) {
  var msg = null; 

  $.ajax({
    async: false, 
    type: "GET",
    url: url, 
    contentType: "application/json",
    dataType: "json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;  
};

ArangoConnection.prototype.GET = ArangoConnection.prototype.get;

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a delete request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype._delete = function (url) {
  var msg = null; 

  $.ajax({
    async: false, 
    type: "DELETE",
    url: url, 
    contentType: "application/json",
    dataType: "json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;  
};

ArangoConnection.prototype.DELETE = ArangoConnection.prototype._delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a post request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.post = function (url, body) {
  var msg = null;

  $.ajax({
    async: false, 
    type: "POST",
    url: url, 
    data: body, 
    contentType: "application/json",
    dataType: "json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;  
};

ArangoConnection.prototype.POST = ArangoConnection.prototype.post;

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a put request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.put = function (url, body) {
  var msg = null; 

  $.ajax({
    async: false, 
    type: "PUT",
    url: url, 
    data: body, 
    contentType: "application/json",
    dataType: "json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;  
};

ArangoConnection.prototype.PUT = ArangoConnection.prototype.put;

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a patch request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.patch = function (url, body) {
  var msg = null; 

  $.ajax({
    async: false, 
    type: "PATCH",
    url: url, 
    data: body, 
    contentType: "application/json",
    dataType: "json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;  
};

ArangoConnection.prototype.PATCH = ArangoConnection.prototype.patch;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

(function () {

  var internal = Module.prototype.ModuleCache["/internal"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief global server connection
////////////////////////////////////////////////////////////////////////////////

  internal.arango = new ArangoConnection();

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
/// @brief global output buffer
////////////////////////////////////////////////////////////////////////////////

  internal.browserOutputBuffer = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

  internal.output = function () {
    var i;

    for (i = 0;  i < arguments.length;  ++i) {
      var value = arguments[i];
      var text;

      if (value === null) {
        text = "null";
      }
      else if (value === undefined) {
        text = "undefined";
      }
      else if (typeof(value) === "object") {
        try {
          text = JSON.stringify(value);
        }
        catch (err) {
          text = String(value);
        }
      }
      else {
        text = String(value);
      }

      internal.browserOutputBuffer += text; 
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to browser window
////////////////////////////////////////////////////////////////////////////////

  internal.printBrowser = function () {
    internal.printShell.apply(internal.printShell, arguments);

    jqconsole.Write('==> ' + internal.browserOutputBuffer + '\n', 'jssuccess');
    internal.browserOutputBuffer = "";
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                  global functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing
////////////////////////////////////////////////////////////////////////////////

function start_pretty_print () {
  require("internal").startPrettyPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_pretty_print () {
  require("internal").stopPrettyPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing with optional color
////////////////////////////////////////////////////////////////////////////////

function start_color_print (color) {
  require("internal").startColorPrint(color, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_color_print () {
  require("internal").stopColorPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}"
// End:
