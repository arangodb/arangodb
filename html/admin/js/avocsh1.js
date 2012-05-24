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
/// @brief normalises a path
////////////////////////////////////////////////////////////////////////////////

Module.prototype.normalise = function (path) {
  var p;
  var q;
  var x;

  if (path == "") {
    return this.id;
  }

  p = path.split('/');

  // relative path
  if (p[0] == "." || p[0] == "..") {
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

  for (var i = 0;  i < q.length;  ++i) {
    x = q[i];

    if (x == "") {
    }
    else if (x == ".") {
    }
    else if (x == "..") {
      if (n.length == 0) {
        throw "cannot cross module top";
      }

      n.pop();
    }
    else {
      n.push(x);
    }
  }

  return "/" + n.join('/');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a definition or creates a new module descriptor
////////////////////////////////////////////////////////////////////////////////

Module.prototype.require = function (path) {
  var module;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (path in ModuleCache) {
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
  if (! (path in ModuleCache)) {
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
/// @brief executs a get request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.get = function (url) {
  var msg; 

  $.ajax({
    async: false, 
    type: "GET",
    url: url, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      msg = data; 
    }
  });

  return msg;  
};

ArangoConnection.prototype.GET = ArangoConnection.prototype.get;

////////////////////////////////////////////////////////////////////////////////
/// @brief executs a delete request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.delete = function (url) {
  var msg; 

  $.ajax({
    async: false, 
    type: "DELETE",
    url: url, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      msg = data;
    }
  });

  return msg;  
};


ArangoConnection.prototype.DELETE = ArangoConnection.prototype.delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief executs a post request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.post = function (url, body) {
  var msg;

  $.ajax({
    async: false, 
    type: "POST",
    url: url, 
    data: body, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      msg = data;
    }
  });

  return msg;  
};

ArangoConnection.prototype.POST = ArangoConnection.prototype.post;

////////////////////////////////////////////////////////////////////////////////
/// @brief executs a put request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.put = function (url, body) {
  var msg; 

  $.ajax({
    async: false, 
    type: "PUT",
    url: url, 
    data: body, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      msg = data;
    }
  });

  return msg;  
};

ArangoConnection.prototype.PUT = ArangoConnection.prototype.put;

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
/// @brief module cache
////////////////////////////////////////////////////////////////////////////////

var ModuleCache = {};
var module = new Module("/");

ModuleCache["/"] = module;
ModuleCache["/internal"] = new Module("/internal");

var internal = ModuleCache["/internal"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief global server connection
////////////////////////////////////////////////////////////////////////////////

var arango = new ArangoConnection();

////////////////////////////////////////////////////////////////////////////////
/// @brief global output buffer
////////////////////////////////////////////////////////////////////////////////

internal.browserOutputBuffer = "";

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
/// @brief starts pager (dummy)
////////////////////////////////////////////////////////////////////////////////

function SYS_START_PAGER () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops pager (dummy)
////////////////////////////////////////////////////////////////////////////////

function SYS_STOP_PAGER () {
}

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
                  })(); 

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

function TRI_SYS_OUTPUT () {
  for (var i = 0;  i < arguments.length;  ++i) {
    var value = arguments[i];
    var text;

    if (value === null) {
      text = "null";
    }
    else if (value === undefined) {
      text = "undefined";
    }
    else if (typeof(value) == "object") {
      try {
        text = JSON.stringify(value);
      }
      catch (err) {
        text = "" + value;
      }
    }
    else {
      text = "" + value;
    }

    internal.browserOutputBuffer += escapeHTML(text);
  }

  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

// must be a variable definition for the browser
internal.printBrowser = function () {
  for (var i = 0;  i < arguments.length;  ++i) {
    if (0 < i) {
      TRI_SYS_OUTPUT(" ");
    }

    if (typeof(arguments[i]) === "string") {
      TRI_SYS_OUTPUT(arguments[i]);      
    } 
    else {
      TRI_PRINT(arguments[i], [], "~", [], 0);
    }
  }

  TRI_SYS_OUTPUT("\n");

  // flush buffer
  $('#avocshWindow').append('<p class="avocshSuccess">' + internal.browserOutputBuffer + '</p>'); 
  internal.browserOutputBuffer = "";

  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief global require function
////////////////////////////////////////////////////////////////////////////////

function require (path) {
  return module.require(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
