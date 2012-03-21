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
  var raw;
  var content;
  var sandbox;
  var paths;
  var module;
  var f;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (path in ModuleCache) {
    return ModuleCache[path].exports;
  }

  // locate file and read content
  raw = internal.readFile(path);

  // create a new sandbox and execute
  ModuleCache[path] = module = new Module(path);

  content = "(function (module, exports, require, print) {" + raw.content + "\n/* end-of-file '" + raw.path + "' */ });";

  try {
    f = SYS_EXECUTE(content, undefined, path);
  }
  catch (err) {
    CONSOLE_ERROR("in file %s: %o", path, err.stack);
    throw err;
  }

  if (f == undefined) {
    throw "cannot create context function";
  }

  f(module, module.exports, function(path) { return module.require(path); }, print);

  return module.exports;
};

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
/// @brief top-level module
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/"] = module = new Module("/");

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
/// @verbinclude modules1
///
/// Then you can use require to load the file and access the exports.
///
/// @verbinclude modules2
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
ModuleCache["/fs"].exports.exists = FS_EXISTS;
fs = ModuleCache["/fs"].exports;

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
ModuleCache["/internal"].exports.execute = SYS_EXECUTE;
ModuleCache["/internal"].exports.load = SYS_LOAD;
ModuleCache["/internal"].exports.log = SYS_LOG;
ModuleCache["/internal"].exports.logLevel = SYS_LOG_LEVEL;
ModuleCache["/internal"].exports.output = SYS_OUTPUT;
ModuleCache["/internal"].exports.processStat = SYS_PROCESS_STAT;
ModuleCache["/internal"].exports.read = SYS_READ;
ModuleCache["/internal"].exports.sprintf = SYS_SPRINTF;
ModuleCache["/internal"].exports.time = SYS_TIME;
internal = ModuleCache["/internal"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file
////////////////////////////////////////////////////////////////////////////////

internal.readFile = function (path) {

  // try to load the file
  var paths = MODULES_PATH;

  for (var i = 0;  i < paths.length;  ++i) {
    var p = paths[i];
    var n;

    if (p == "") {
      n = "." + path + ".js"
    }
    else {
      n = p + "/" + path + ".js";
    }

    if (FS_EXISTS(n)) {
      return { path : n, content : SYS_READ(n) };
    }
  }

  throw "cannot find a file named '" + path + "' using the module path(s) '" + MODULES_PATH + "'";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a file
////////////////////////////////////////////////////////////////////////////////

internal.loadFile = function (path) {

  // try to load the file
  var paths = MODULES_PATH;

  for (var i = 0;  i < paths.length;  ++i) {
    var p = paths[i];
    var n;

    if (p == "") {
      n = "." + path + ".js"
    }
    else {
      n = p + "/" + path + ".js";
    }

    if (FS_EXISTS(n)) {
      return SYS_LOAD(n);
    }
  }

  throw "cannot find a file named '" + path + "' using the module path(s) '" + MODULES_PATH + "'";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  Module "console"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleConsole
/// @{
////////////////////////////////////////////////////////////////////////////////

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

function CONSOLE_DEBUG () {
  var msg;

  msg = internal.sprintf.apply(internal.sprintf, arguments);
  internal.log("debug", msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs error message
///
/// @FUN{console.error(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// error message.
////////////////////////////////////////////////////////////////////////////////

function CONSOLE_ERROR () {
  var msg;

  msg = internal.sprintf.apply(internal.sprintf, arguments);
  internal.log("error", msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs info message
///
/// @FUN{console.info(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// info message.
////////////////////////////////////////////////////////////////////////////////

function CONSOLE_INFO () {
  var msg;

  msg = internal.sprintf.apply(internal.sprintf, arguments);
  internal.log("info", msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs log message
///
/// @FUN{console.log(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// log message.
////////////////////////////////////////////////////////////////////////////////

function CONSOLE_LOG () {
  var msg;

  msg = internal.sprintf.apply(internal.sprintf, arguments);
  internal.log("info", msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs warn message
///
/// @FUN{console.warn(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// warn message.
////////////////////////////////////////////////////////////////////////////////

function CONSOLE_WARN () {
  var msg;

  msg = internal.sprintf.apply(internal.sprintf, arguments);
  internal.log("warn", msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief console module
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/console"] = new Module("/console");
ModuleCache["/console"].exports.debug = CONSOLE_DEBUG;
ModuleCache["/console"].exports.error = CONSOLE_ERROR;
ModuleCache["/console"].exports.info = CONSOLE_INFO;
ModuleCache["/console"].exports.log = CONSOLE_LOG;
ModuleCache["/console"].exports.warn = CONSOLE_WARN;
console = ModuleCache["/console"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
