/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, nonpropdel: true, proto: true */
/*global require, module, Module, FS_MOVE, FS_REMOVE, FS_EXISTS, FS_IS_DIRECTORY, FS_LIST_TREE, 
  SYS_EXECUTE, SYS_LOAD, SYS_LOG, SYS_LOG_LEVEL, SYS_OUTPUT, SYS_PROCESS_STAT, SYS_READ,
  SYS_SPRINTF, SYS_TIME, SYS_START_PAGER, SYS_STOP_PAGER, SYS_SHA256, SYS_WAIT, SYS_GETLINE,
  SYS_PARSE, ARANGO_QUIET, MODULES_PATH, COLOR_OUTPUT, COLOR_OUTPUT_RESET, COLOR_BRIGHT,
  COLOR_BLACK, COLOR_BOLD_BLACK, COLOR_BLINK, COLOR_BLUE, COLOR_BOLD_BLUE, COLOR_BOLD_GREEN,
  COLOR_RED, COLOR_BOLD_RED, COLOR_GREEN, COLOR_WHITE, COLOR_BOLD_WHITE, COLOR_YELLOW,
  COLOR_BOLD_YELLOW, PRETTY_PRINT  */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
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

(function () {
  var internal = Module.prototype.ModuleCache["/internal"].exports;

  // system functions
  if (typeof SYS_EXECUTE !== "undefined") {
    internal.execute = SYS_EXECUTE;
    delete SYS_EXECUTE;
  }

  if (typeof SYS_GETLINE !== "undefined") {
    internal.getline = SYS_GETLINE;
    delete SYS_GETLINE;
  }

  if (typeof SYS_LOAD !== "undefined") {
    internal.load = SYS_LOAD;
    delete SYS_LOAD;
  }

  if (typeof SYS_LOG !== "undefined") {
    internal.log = SYS_LOG;
    delete SYS_LOG;
  }

  if (typeof SYS_LOG_LEVEL !== "undefined") {
    internal.logLevel = SYS_LOG_LEVEL;
    delete SYS_LOG_LEVEL;
  }

  if (typeof SYS_OUTPUT !== "undefined") {
    internal.output = SYS_OUTPUT;
    delete SYS_OUTPUT;
  }

  if (typeof SYS_PARSE !== "undefined") {
    internal.parse= SYS_PARSE;
    delete SYS_PARSE;
  }

  if (typeof SYS_PROCESS_STAT !== "undefined") {
    internal.processStat = SYS_PROCESS_STAT;
    delete SYS_PROCESS_STAT;
  }

  if (typeof SYS_READ !== "undefined") {
    internal.read = SYS_READ;
    delete SYS_READ;
  }

  if (typeof SYS_SHA256 !== "undefined") {
    internal.sha256 = SYS_SHA256;
    delete SYS_SHA256;
  }

  if (typeof SYS_SPRINTF !== "undefined") {
    internal.sprintf = SYS_SPRINTF;
    delete SYS_SPRINTF;
  }

  if (typeof SYS_TIME !== "undefined") {
    internal.time = SYS_TIME;
    delete SYS_TIME;
  }

  if (typeof SYS_WAIT !== "undefined") {
    internal.wait = SYS_WAIT;
    delete SYS_WAIT;
  }

  if (typeof FS_EXISTS !== "undefined") {
    internal.exists = FS_EXISTS;
    delete FS_EXISTS;
  }

  if (typeof FS_IS_DIRECTORY !== "undefined") {
    internal.isDirectory = FS_IS_DIRECTORY;
    delete FS_IS_DIRECTORY;
  }

  if (typeof FS_LIST_TREE !== "undefined") {
    internal.listTree = FS_LIST_TREE;
    delete FS_LIST_TREE;
  }

  if (typeof FS_MOVE !== "undefined") {
    internal.move = FS_MOVE;
    delete FS_MOVE;
  }

  if (typeof FS_REMOVE !== "undefined") {
    internal.remove = FS_REMOVE;
    delete FS_REMOVE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleInternal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief modules path
////////////////////////////////////////////////////////////////////////////////

  internal.MODULES_PATH = "";

  if (typeof MODULES_PATH !== "undefined") {
    internal.MODULES_PATH = MODULES_PATH;
    delete MODULES_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet flag
////////////////////////////////////////////////////////////////////////////////

  internal.ARANGO_QUIET = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief pretty print flag
////////////////////////////////////////////////////////////////////////////////

  internal.PRETTY_PRINT = false;

  if (typeof PRETTY_PRINT !== "undefined") {
    internal.PRETTY_PRINT = PRETTY_PRINT;
    delete PRETTY_PRINT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief color constants
////////////////////////////////////////////////////////////////////////////////
  
  internal.COLORS = { };

  if (typeof COLORS !== "undefined") {
    internal.COLORS = COLORS;
    delete COLORS;
  }
  else {
    [ 'COLOR_RED', 'COLOR_BOLD_RED', 'COLOR_GREEN', 'COLOR_BOLD_GREEN', 'COLOR_YELLOW', 'COLOR_BOLD_YELLOW', 'COLOR_WHITE', 'COLOR_BOLD_WHITE', 'COLOR_BLACK', 'COLOR_BOLD_BLACK', 'COLOR_BLINK', 'COLOR_BRIGHT', 'COLOR_RESET' ].forEach(function(color) {
      internal.COLORS[color] = '';
    });
  }

  internal.COLORS.COLOR_PUNCTUATION = internal.COLORS.COLOR_RESET;
  internal.COLORS.COLOR_STRING = internal.COLORS.COLOR_WHITE;
  internal.COLORS.COLOR_NUMBER = internal.COLORS.COLOR_BOLD_WHITE;
  internal.COLORS.COLOR_INDEX = internal.COLORS.COLOR_BOLD_WHITE;
  internal.COLORS.COLOR_TRUE = internal.COLORS.COLOR_BOLD_WHITE;
  internal.COLORS.COLOR_FALSE = internal.COLORS.COLOR_BOLD_WHITE;
  internal.COLORS.COLOR_NULL = internal.COLORS.COLOR_BOLD_WHITE;

  internal.NOCOLORS = { };
  for (var i in internal.COLORS) {
    if (internal.COLORS.hasOwnProperty(i)) {
      internal.NOCOLORS[i] = '';
    }
  }
 
  internal.COLOR_OUTPUT = false;

  if (typeof COLOR_OUTPUT !== "undefined") {
    internal.COLOR_OUTPUT = COLOR_OUTPUT;
    delete COLOR_OUTPUT;
  }

  internal.colors = (internal.COLOR_OUTPUT ? internal.COLORS : internal.NOCOLORS);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleInternal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output
///
/// @FUN{internal.printShell(@FA{arg1}, @FA{arg2}, @FA{arg3}, ...)}
///
/// Only available in shell mode.
///
/// Prints the arguments. If an argument is an object having a
/// function @FN{_PRINT}, then this function is called. Otherwise @FN{toJson} is
/// used.  A final newline is printed
///
/// @verbinclude fluent40
////////////////////////////////////////////////////////////////////////////////

  internal.printShell = function () {
    var i;

    for (i = 0;  i < arguments.length;  ++i) {
      if (0 < i) {
        internal.output(" ");
      }

      if (typeof(arguments[i]) === "string") {
        internal.output(arguments[i]);
      }
      else {
        internal.printRecursive(arguments[i], [], "~", [], 0);
      }
    }
    
    internal.output("\n");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief quote cache
////////////////////////////////////////////////////////////////////////////////

  internal.characterQuoteCache = {
    '\b': '\\b', // ASCII 8, Backspace
    '\t': '\\t', // ASCII 9, Tab
    '\n': '\\n', // ASCII 10, Newline
    '\f': '\\f', // ASCII 12, Formfeed
    '\r': '\\r', // ASCII 13, Carriage Return
    '\"': '\\"',
    '\\': '\\\\'
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a single character
////////////////////////////////////////////////////////////////////////////////

  internal.quoteSingleJsonCharacter = function (c) {
    if (internal.characterQuoteCache.hasOwnProperty[c]) {
      return internal.characterQuoteCache[c];
    }

    var charCode = c.charCodeAt(0);
    var result;

    if (charCode < 16) {
      result = '\\u000';
    }
    else if (charCode < 256) {
      result = '\\u00';
    }
    else if (charCode < 4096) {
      result = '\\u0';
    }
    else {
      result = '\\u';
    }

    result += charCode.toString(16);
    internal.characterQuoteCache[c] = result;

    return result;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a string character
////////////////////////////////////////////////////////////////////////////////

  internal.quoteJsonString = function (str) {
    var quotable = /[\\\"\x00-\x1f]/g;
    return '"' + str.replace(quotable, internal.quoteSingleJsonCharacter) + '"';
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output without a new-line
////////////////////////////////////////////////////////////////////////////////

  internal.printRecursive = function (value, seen, path, names, level) {
    var p;

    if (seen === undefined) {
      seen = [];
      names = [];
    }

    p = seen.indexOf(value);

    if (0 <= p) {
      internal.output(names[p]);
    }
    else {
      if (value instanceof Object) {
        seen.push(value);
        names.push(path);
      }

      if (value instanceof Object) {
        if (typeof value._PRINT === "function") {
          value._PRINT(seen, path, names, level);
        }
        else if (value instanceof Array) {
          internal.printArray(value, seen, path, names, level);
        }
        else if (value.__proto__ === Object.prototype) {
          internal.printObject(value, seen, path, names, level);
        }
	else if (typeof value.toString === "function") {
          internal.output(value.toString());
        }
        else {
          internal.printObject(value, seen, path, names, level);
        }
      }
      else if (value === undefined) {
        internal.output("undefined");
      }
      else {
        if (typeof(value) === "string") {
          internal.output(internal.colors.COLOR_STRING);
          internal.output(internal.quoteJsonString(value));
          internal.output(internal.colors.COLOR_RESET);
        }
        else if (typeof(value) === "boolean") {
          internal.output(value ? internal.colors.COLOR_TRUE : internal.colors.COLOR_FALSE);
          internal.output(String(value));
          internal.output(internal.colors.COLOR_RESET);
        }
        else if (typeof(value) === "number") {
          internal.output(internal.colors.COLOR_NUMBER);
          internal.output(String(value));
          internal.output(internal.colors.COLOR_RESET);
        }
        else if (value == null) {
          internal.output(internal.colors.COLOR_NULL);
          internal.output(String(value));
          internal.output(internal.colors.COLOR_RESET);
        }
        else {
          internal.output(String(value));
        }
      }
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

  internal.printArray = function (object, seen, path, names, level) {
    if (object.length === 0) {
      internal.output(internal.colors.COLOR_PUNCTUATION);
      internal.output("[ ]");
      internal.output(internal.colors.COLOR_RESET);
    }
    else {
      var i;
      var sep = "";

      internal.output(internal.colors.COLOR_PUNCTUATION);
      internal.output("[");
      internal.output(internal.colors.COLOR_RESET);

      var newLevel = level + 1;

      for (i = 0;  i < object.length;  i++) {
        internal.output(internal.colors.COLOR_PUNCTUATION);
        internal.output(sep);
        internal.output(internal.colors.COLOR_RESET);

        internal.printIndent(newLevel);

        internal.printRecursive(object[i],
                                seen,
                                path + "[" + i + "]",
                                names,
                                newLevel);
        sep = ", ";
      }

      if (object.length > 1) {
        internal.output(" ");
      }

      internal.printIndent(level);

      internal.output(internal.colors.COLOR_PUNCTUATION);
      internal.output("]");
      internal.output(internal.colors.COLOR_RESET);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an object
////////////////////////////////////////////////////////////////////////////////

  internal.printObject = function (object, seen, path, names, level) {
    var sep = " ";
    var k;

    internal.output(internal.colors.COLOR_PUNCTUATION);
    internal.output("{");
    internal.output(internal.colors.COLOR_RESET);

    var newLevel = level + 1;

    for (k in object) {
      if (object.hasOwnProperty(k)) {
        var val = object[k];

        internal.output(internal.colors.COLOR_PUNCTUATION);
        internal.output(sep);
        internal.output(internal.colors.COLOR_RESET);

        internal.printIndent(newLevel);

        internal.output(internal.colors.COLOR_INDEX);
        internal.output(internal.quoteJsonString(k));
        internal.output(internal.colors.COLOR_RESET);
        internal.output(" : ");

        internal.printRecursive(val,
                                seen,
                                path + "[" + k + "]",
                                names,
                                newLevel);
        sep = ", ";
      }
    }

    if (sep === ", ") {
      internal.output(" ");
    }

    internal.printIndent(level);

    internal.output(internal.colors.COLOR_PUNCTUATION);
    internal.output("}");
    internal.output(internal.colors.COLOR_RESET);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the ident for pretty printing
////////////////////////////////////////////////////////////////////////////////

  internal.printIndent = function (level) {
    var j;

    if (internal.PRETTY_PRINT) {
      internal.output("\n");

      for (j = 0; j < level; ++j) {
        internal.output("  ");
      }
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the module cache
////////////////////////////////////////////////////////////////////////////////

  internal.flushModuleCache = function() {
    module.unloadAll();
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleInternal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global print
////////////////////////////////////////////////////////////////////////////////

  internal.print = internal.printShell;

  if (typeof internal.printBrowser === "function") {
    internal.print = internal.printBrowser;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief global print
////////////////////////////////////////////////////////////////////////////////

  internal.printf = function () {
    var text = internal.sprintf.apply(internal.springf, arguments);
    internal.output(text);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief start pager
////////////////////////////////////////////////////////////////////////////////

  internal.start_pager = function () {};

  if (typeof SYS_START_PAGER !== "undefined") {
    internal.start_pager = SYS_START_PAGER;
    delete SYS_START_PAGER;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pager
////////////////////////////////////////////////////////////////////////////////

  internal.stop_pager = function () {};

  if (typeof SYS_STOP_PAGER !== "undefined") {
    internal.stop_pager = SYS_STOP_PAGER;
    delete SYS_STOP_PAGER;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing
////////////////////////////////////////////////////////////////////////////////
 
  internal.startPrettyPrint = function () {
    if (! internal.PRETTY_PRINT) {
      internal.PRETTY_PRINT = true;
      internal.print("using pretty printing");
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

  internal.stopPrettyPrint = function () {
    if (internal.PRETTY_PRINT) {
      internal.PRETTY_PRINT = false;
      internal.print("disabled pretty printing");
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief start color printing
////////////////////////////////////////////////////////////////////////////////

  internal.startColorPrint = function () {
    internal.colors = internal.COLORS;
    if (! internal.COLOR_OUTPUT) {
      internal.COLOR_OUTPUT = true;
      internal.print("starting color printing"); 
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stop color printing
////////////////////////////////////////////////////////////////////////////////

  internal.stopColorPrint = function () {
    if (internal.COLOR_OUTPUT) {
      internal.print("disabled color printing");
    }
    internal.COLOR_OUTPUT = false;
    internal.colors = internal.NOCOLORS;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief encode password using SHA256
////////////////////////////////////////////////////////////////////////////////

  internal.encodePassword = function (password) {
    var salt;
    var encoded;

    salt = internal.sha256("time:" + internal.time());
    salt = salt.substr(0,8);

    encoded = "$1$" + salt + "$" + internal.sha256(salt + password);
    
    return encoded;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief extends a prototype
////////////////////////////////////////////////////////////////////////////////

  internal.extend = function (target, source) {
    Object.getOwnPropertyNames(source)
      .forEach(function(propName) {
        Object.defineProperty(target, propName,
			      Object.getOwnPropertyDescriptor(source, propName));
      });

    return target;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file from the module path or the database
////////////////////////////////////////////////////////////////////////////////

  internal.loadDatabaseFile = function (path) {
    var i;
    var mc;
    var n;

    // try to load the file
    var paths = internal.MODULES_PATH;

    for (i = 0;  i < paths.length;  ++i) {
      var p = paths[i];

      if (p === "") {
        n = "." + path + ".js";
      }
      else {
        n = p + "/" + path + ".js";
      }

      if (internal.exists(n)) {
        Module.prototype.ModuleExistsCache[path] = true;
        return { path : n, content : internal.read(n) };
      }
    }

    // try to load the module from the database
    if (internal.db !== undefined) {
      mc = internal.db._collection("_modules");

      if (mc !== null && typeof mc.firstExample === "function") {
	n = mc.firstExample({ path: path });

	if (n !== null) {
          if (n.hasOwnProperty('content')) {
            Module.prototype.ModuleExistsCache[path] = true;
            return { path : "_collection/" + path, content : n.content };
          }

	  if (Module.prototype.ModuleExistsCache.hasOwnProperty("/console")) {
	    var console = Module.prototype.ModuleExistsCache["/console"];
	    console.error("found empty content in '%s'", JSON.stringify(n));
	  }
	}
      }
    }

    Module.prototype.ModuleExistsCache[path] = false;

    throw "cannot find a file named '"
        + path
        + "' using the module path(s) '" 
        + internal.MODULES_PATH + "'";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a file from the file-system
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

      if (internal.exists(n)) {
        return internal.load(n);
      }
    }

    throw "cannot find a file named '"
        + path 
        + "' using the module path(s) '" 
        + internal.MODULES_PATH + "'";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a module
////////////////////////////////////////////////////////////////////////////////

  internal.defineModule = function (path, file) {
    var content;
    var m;
    var mc;

    content = internal.read(file);

    mc = internal.db._collection("_modules");

    if (mc === null) {
      mc = internal.db._create("_modules", { isSystem: true });
    }

    path = module.normalise(path);
    m = mc.firstExample({ path: path });

    if (m === null) {
      mc.save({ path: path, content: content });
    }
    else {
      mc.replace(m, { path: path, content: content });
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
