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

////////////////////////////////////////////////////////////////////////////////
/// @page JSModulesTOC
///
/// <ol>
///   <li>@ref JSModulesRequire "require"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModules JavaScript Modules
///
/// The AvocadoDB uses a <a
/// href="http://wiki.commonjs.org/wiki/Modules">CommonJS</a> compatible module
/// concept. You can use the function @FN{require} in order to load a
/// module. @FN{require} returns the exported variables and functions of the
/// module. You can use the option @CO{startup.modules-path} to specify the
/// location of the JavaScript files.
///
/// <hr>
/// @copydoc JSModulesTOC
/// <hr>
///
/// @anchor JSModulesRequire
/// @copydetails JSF_require
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
  var sandbox;
  var paths;
  var module;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (path in ModuleCache) {
    return ModuleCache[path].exports;
  }

  // try to load the file
  paths = MODULES_PATH.split(";");
  content = undefined;

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
      content = SYS_READ(n);
      break;
    }
  }

  if (content == undefined) {
    throw "cannot find a module named '" + path + "' using the module path(s) '" + MODULES_PATH + "'";
  }

  // create a new sandbox and execute
  ModuleCache[path] = module = new Module(path);

  sandbox = {};
  sandbox.module = module;
  sandbox.exports = module.exports;
  sandbox.require = function(path) { return sandbox.module.require(path); }
  sandbox.print = print;

  SYS_EXECUTE(content, sandbox, path);

  module.exports = sandbox.exports;

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
/// @page JSModuleFsTOC
///
/// <ol>
///   <li>@ref JSModuleFsExists "fs.exists"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleFs Module "fs"
///
/// The implementation follows the CommonJS specification
/// <a href="http://wiki.commonjs.org/wiki/Filesystem/A/0">Filesystem/A/0</a>.
///
/// <hr>
/// @copydoc JSModuleFsTOC
/// <hr>
///
/// @anchor JSModuleFsExists
/// @copydetails JS_Exists
////////////////////////////////////////////////////////////////////////////////

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
/// @page JSModuleInternalTOC
///
/// <ol>
///   <li>@ref JSModuleInternalExecute "internal.execute"</li>
///   <li>@ref JSModuleInternalLoad "internal.load"</li>
///   <li>@ref JSModuleInternalLogLevel "internal.log"</li>
///   <li>@ref JSModuleInternalLogLevel "internal.logLevel"</li>
///   <li>@ref JSModuleInternalRead "internal.read"</li>
///   <li>@ref JSModuleInternalSPrintF "internal.sprintf"</li>
///   <li>@ref JSModuleInternalTime "internal.time"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleInternal Module "internal"
///
/// The following functions are used internally.
///
/// <hr>
/// @copydoc JSModuleInternalTOC
/// <hr>
///
/// @anchor JSModuleInternalExecute
/// @copydetails JS_Execute
///
/// @anchor JSModuleInternalLoad
/// @copydetails JS_Load
///
/// @anchor JSModuleInternalLog
/// @copydetails JS_Log
///
/// @anchor JSModuleInternalLogLevel
/// @copydetails JS_LogLevel
///
/// @anchor JSModuleInternalRead
/// @copydetails JS_Read
///
/// @anchor JSModuleInternalSPrintF
/// @copydetails JS_SPrintF
///
/// @anchor JSModuleInternalTime
/// @copydetails JS_Time
////////////////////////////////////////////////////////////////////////////////

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
ModuleCache["/internal"].exports.read = SYS_READ;
ModuleCache["/internal"].exports.sprintf = SYS_SPRINTF;
ModuleCache["/internal"].exports.time = SYS_TIME;
internal = ModuleCache["/internal"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  Module "console"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleConsoleTOC
///
/// <ol>
///   <li>@ref JSModuleConsoleDebug "console.debug"</li>
///   <li>@ref JSModuleConsoleError "console.error"</li>
///   <li>@ref JSModuleConsoleInfo "console.info"</li>
///   <li>@ref JSModuleConsoleLog "console.log"</li>
///   <li>@ref JSModuleConsoleWarn "console.warn"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleConsole Module "console"
///
/// The implementation follows the CommonJS specification
/// <a href="http://wiki.commonjs.org/wiki/Console">Console</a>.
///
/// <hr>
/// @copydoc JSModuleConsoleTOC
/// <hr>
///
/// @anchor JSModuleConsoleDebug
/// @copydetails JSF_CONSOLE_DEBUG
///
/// @anchor JSModuleConsoleError
/// @copydetails JSF_CONSOLE_ERROR
///
/// @anchor JSModuleConsoleInfo
/// @copydetails JSF_CONSOLE_INFO
///
/// @anchor JSModuleConsoleLog
/// @copydetails JSF_CONSOLE_LOG
///
/// @anchor JSModuleConsoleWarn
/// @copydetails JSF_CONSOLE_WARN
////////////////////////////////////////////////////////////////////////////////

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
