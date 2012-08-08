/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global require, module, ModuleCache, SYS_EXECUTE, CONSOLE_ERROR,
 FS_MOVE, FS_REMOVE, FS_EXISTS, 
 SYS_LOAD, SYS_LOG, SYS_LOG_LEVEL, SYS_OUTPUT,
 SYS_PROCESS_STAT, SYS_READ, SYS_SPRINTF, SYS_TIME,
 SYS_START_PAGER, SYS_STOP_PAGER, ARANGO_QUIET, MODULES_PATH,
 COLOR_OUTPUT, COLOR_OUTPUT_RESET, COLOR_BRIGHT, PRETTY_PRINT */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript server functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            Module
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Module
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief module cache
////////////////////////////////////////////////////////////////////////////////

ModuleCache = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief module constructor
////////////////////////////////////////////////////////////////////////////////

function Module (id) {
  this.id = id;
  this.exports = {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a file and creates a new module descriptor
////////////////////////////////////////////////////////////////////////////////

Module.prototype.require = function (path) {
  var content;
  var f;
  var module;
  var paths;
  var raw;
  var sandbox;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (ModuleCache.hasOwnProperty(path)) {
    return ModuleCache[path].exports;
  }

  // locate file and read content
  raw = ModuleCache["/internal"].exports.readFile(path);

  // create a new sandbox and execute
  module = ModuleCache[path] = new Module(path);

  content = "(function (module, exports, require, print) {"
          + raw.content 
          + "\n});";

  try {
    f = SYS_EXECUTE(content, undefined, path);
  }
  catch (err) {
    require("console").error("in file %s: %o", path, err.stack);
    throw err;
  }

  if (f === undefined) {
    throw "cannot create context function";
  }

  f(module,
    module.exports,
    function(path) { return module.require(path); },
    ModuleCache["/internal"].exports.print);

  return module.exports;
};

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
/// @brief unloads module
////////////////////////////////////////////////////////////////////////////////

Module.prototype.unload = function (path) {
  if (! path) {
    return;
  }

  var norm = module.normalise(path);

  if (   norm === "/"
      || norm === "/internal"
      || norm === "/console"
      || norm === "/fs") {
    return;
  }

  delete ModuleCache[norm];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief top-level module
////////////////////////////////////////////////////////////////////////////////

module = ModuleCache["/"] = new Module("/");

////////////////////////////////////////////////////////////////////////////////
/// @brief global require function
///
/// @FUN{require(@FA{path})}
///
/// @FN{require} checks if the file specified by @FA{path} has already been
/// loaded.  If not, the content of the file is executed in a new
/// context. Within the context you can use the global variable @CODE{exports}
/// in order to export variables and functions. This variable is returned by
/// @FN{require}.
///
/// Assume that your module file is @CODE{test1.js} and contains
///
/// @verbinclude modules-require-1
///
/// Then you can use @FN{require} to load the file and access the exports.
///
/// @verbinclude modules-require-2
///
/// @FN{require} follows the specification
/// <a href="http://wiki.commonjs.org/wiki/Modules/1.1.1">Modules/1.1.1</a>.
////////////////////////////////////////////////////////////////////////////////

function require (path) {
  return module.require(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       Module "fs"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleFS
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fs module
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/fs"] = new Module("/fs");

(function () {
  var fs = ModuleCache["/fs"].exports;

  fs.exists = FS_EXISTS;
  fs.move = FS_MOVE;
  fs.remove = FS_REMOVE;
}());

////////////////////////////////////////////////////////////////////////////////
/// @}
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

ModuleCache["/internal"] = new Module("/internal");

(function () {
  var internal = ModuleCache["/internal"].exports;
  var fs = ModuleCache["/fs"].exports;

  // system functions
  internal.execute = SYS_EXECUTE;
  internal.load = SYS_LOAD;
  internal.log = SYS_LOG;
  internal.logLevel = SYS_LOG_LEVEL;
  internal.output = SYS_OUTPUT;
  internal.processStat = SYS_PROCESS_STAT;
  internal.read = SYS_READ;
  internal.sprintf = SYS_SPRINTF;
  internal.time = SYS_TIME;
  internal.sha256 = SYS_SHA256;
  internal.wait = SYS_WAIT;


  // command line parameter
  internal.MODULES_PATH = "";

  if (typeof MODULES_PATH !== "undefined") {
    internal.MODULES_PATH = MODULES_PATH;
  }


  // output 
  internal.start_pager = function() {};
  internal.stop_pager = function() {};

  internal.ARANGO_QUIET = false;

  internal.COLOR_OUTPUT = undefined;
  internal.COLOR_OUTPUT_RESET = "";
  internal.COLOR_BRIGHT = "";

  internal.PRETTY_PRINT = false;

  if (typeof SYS_START_PAGER !== "undefined") {
    internal.start_pager = SYS_START_PAGER;
  }

  if (typeof SYS_STOP_PAGER !== "undefined") {
    internal.stop_pager = SYS_STOP_PAGER;
  }

  if (typeof COLOR_OUTPUT !== "undefined") {
    internal.COLOR_OUTPUT = COLOR_OUTPUT;
  }

  if (typeof COLOR_OUTPUT_RESET !== "undefined") {
    internal.COLOR_OUTPUT_RESET = COLOR_OUTPUT_RESET;
  }

  if (typeof COLOR_BRIGHT !== "undefined") {
    internal.COLOR_BRIGHT = COLOR_BRIGHT;
  }

  if (typeof PRETTY_PRINT !== "undefined") {
    internal.PRETTY_PRINT = PRETTY_PRINT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file
////////////////////////////////////////////////////////////////////////////////

  internal.readFile = function (path) {
    var i;

    // try to load the file
    var paths = internal.MODULES_PATH;

    for (i = 0;  i < paths.length;  ++i) {
      var p = paths[i];
      var n;

      if (p === "") {
        n = "." + path + ".js";
      }
      else {
        n = p + "/" + path + ".js";
      }

      if (fs.exists(n)) {
        return { path : n, content : SYS_READ(n) };
      }
    }

    throw "cannot find a file named '"
        + path
        + "' using the module path(s) '" 
        + internal.MODULES_PATH + "'";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a file
////////////////////////////////////////////////////////////////////////////////

  internal.loadFile = function (path) {
    var i;

    // try to load the file
    var paths = internal.MODULES_PATH;

    for (i = 0;  i < paths.length;  ++i) {
      var p = paths[i];
      var n;

      if (p === "") {
        n = "." + path + ".js";
      }
      else {
        n = p + "/" + path + ".js";
      }

      if (fs.exists(n)) {
        return internal.load(n);
      }
    }

    throw "cannot find a file named '"
        + path 
        + "' using the module path(s) '" 
        + internal.MODULES_PATH + "'";
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                  Module "console"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleConsole
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief console module
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/console"] = new Module("/console");

(function () {
  var internal = ModuleCache["/internal"].exports;
  var console = ModuleCache["/console"].exports;

  console.getline = SYS_GETLINE;

////////////////////////////////////////////////////////////////////////////////
/// @brief logs debug message
///
/// @FUN{console.debug(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// debug message.
///
/// String substitution patterns, which can be used in @FA{format}.
///
/// - @LIT{\%s} string
/// - @LIT{\%d}, @LIT{\%i} integer
/// - @LIT{\%f} floating point number
/// - @LIT{\%o} object hyperlink
////////////////////////////////////////////////////////////////////////////////

  console.debug = function () {
    var msg;

    try {
      msg = internal.sprintf.apply(internal.sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    internal.log("debug", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief logs error message
///
/// @FUN{console.error(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// error message.
////////////////////////////////////////////////////////////////////////////////

  console.error = function () {
    var msg;

    try {
      msg = internal.sprintf.apply(internal.sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    internal.log("error", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief logs info message
///
/// @FUN{console.info(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// info message.
////////////////////////////////////////////////////////////////////////////////

  console.info = function () {
    var msg;

    try {
      msg = internal.sprintf.apply(internal.sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    internal.log("info", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief logs log message
///
/// @FUN{console.log(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// log message.
////////////////////////////////////////////////////////////////////////////////

  console.log = function () {
    var msg;

    try {
      msg = internal.sprintf.apply(internal.sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    internal.log("info", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief logs warn message
///
/// @FUN{console.warn(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// warn message.
////////////////////////////////////////////////////////////////////////////////

  console.warn = function () {
    var msg;

    try {
      msg = internal.sprintf.apply(internal.sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    internal.log("warn", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
