/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global module, ModuleCache, $ */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB web browser shell
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

var ModuleCache = {};
var module = ModuleCache["/"] = new Module("/");

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
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a definition or creates a new module descriptor
////////////////////////////////////////////////////////////////////////////////

Module.prototype.require = function (path) {
  var module;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (ModuleCache.hasOwnProperty(path)) {
    module = ModuleCache[path];
  }
  else {
    module = ModuleCache[path] = new Module(path);
  }

  if (module.definition !== null) {
    module.definition(module.exports, module);
    module.definition = null;
  }

  return module.exports;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a new module definition
////////////////////////////////////////////////////////////////////////////////

Module.prototype.define = function (path, definition) {

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (! ModuleCache.hasOwnProperty(path)) {
    ModuleCache[path] = new Module(path);
  }

  ModuleCache[path].definition = definition;
};

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
// --SECTION--                                                  public functions
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

ArangoConnection.prototype.delete = function (url) {
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


ArangoConnection.prototype.DELETE = ArangoConnection.prototype.delete;

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
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global server connection
////////////////////////////////////////////////////////////////////////////////

var arango = new ArangoConnection();

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
/// @brief module cache
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/internal"] = new Module("/internal");

(function () {
  var internal = ModuleCache["/internal"].exports;

  internal.start_pager = function() {};
  internal.stop_pager = function() {};

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

      internal.browserOutputBuffer += escapeHTML(text);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

  internal.printBrowser = function () {
    internal.printShell.apply(internal.printShell, arguments);

    // flush buffer
    $('#shellContent').append('<p class="shellSuccess">'
                              + internal.browserOutputBuffer
                              + '</p>'); 
    internal.browserOutputBuffer = "";
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
/// @brief escapes HTML
////////////////////////////////////////////////////////////////////////////////

var escapeHTML = (function() {
                    var MAP = {
                      '&': '&amp;',
                      '<': '&lt;',
                      '>': '&gt;',
                      '"': '&#34;',
                      "'": '&#39;',
                      "\n": '<br>',
                      " ": '&nbsp;'
                    };

                    var repl = function(c) { return MAP[c]; };

                    return function(s) {
                      return s.replace(/[&<>'"\n ]/g, repl); //'
                    };
                  }()); 

////////////////////////////////////////////////////////////////////////////////
/// @brief global require function
////////////////////////////////////////////////////////////////////////////////

function require (path) {
  return module.require(path);
}

function ARANGO_QUIET (path) {
  return false; 
}
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
