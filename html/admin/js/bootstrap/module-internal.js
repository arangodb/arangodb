/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, nonpropdel: true, proto: true */
/*global require, module, Module, FS_MAKE_DIRECTORY, FS_MOVE, FS_REMOVE, FS_REMOVE_DIRECTORY,
  FS_REMOVE_RECURSIVE_DIRECTORY, FS_EXISTS, FS_IS_DIRECTORY, FS_IS_FILE, FS_FILESIZE, 
  FS_GET_TEMP_FILE, FS_GET_TEMP_PATH, FS_LIST_TREE, FS_UNZIP_FILE, FS_ZIP_FILE, SYS_DOWNLOAD, 
  SYS_EXECUTE, SYS_LOAD, SYS_LOG, SYS_LOG_LEVEL, SYS_MD5, SYS_OUTPUT, SYS_PROCESS_STAT, 
  SYS_RAND, SYS_READ, SYS_SPRINTF, SYS_TIME, SYS_START_PAGER, SYS_STOP_PAGER, SYS_SHA256, SYS_WAIT,
  SYS_GETLINE, SYS_PARSE, SYS_SAVE, SYS_IMPORT_CSV_FILE, SYS_IMPORT_JSON_FILE, PACKAGE_PATH,
  SYS_GEN_RANDOM_NUMBERS, SYS_GEN_RANDOM_ALPHA_NUMBERS, SYS_GEN_RANDOM_SALT, SYS_CREATE_NONCE,
  SYS_CHECK_AND_MARK_NONCE, SYS_REQUEST_STATISTICS, SYS_UNIT_TESTS, SYS_UNIT_TESTS_RESULT:true,
  SYS_PROCESS_CSV_FILE, SYS_PROCESS_JSON_FILE, ARANGO_QUIET, MODULES_PATH, COLORS, COLOR_OUTPUT,
  COLOR_OUTPUT_RESET, COLOR_BRIGHT, COLOR_BLACK, COLOR_BOLD_BLACK, COLOR_BLINK, COLOR_BLUE,
  COLOR_BOLD_BLUE, COLOR_BOLD_GREEN, COLOR_RED, COLOR_BOLD_RED, COLOR_GREEN, COLOR_WHITE,
  COLOR_BOLD_WHITE, COLOR_YELLOW, COLOR_BOLD_YELLOW, COLOR_CYAN, COLOR_BOLD_CYAN, COLOR_MAGENTA,
  COLOR_BOLD_MAGENTA, PRETTY_PRINT, VALGRIND, VERSION, UPGRADE,
  BYTES_SENT_DISTRIBUTION, BYTES_RECEIVED_DISTRIBUTION, CONNECTION_TIME_DISTRIBUTION,
  REQUEST_TIME_DISTRIBUTION, HOME, DEVELOPMENT_MODE, THREAD_NUMBER, PATH_SEPARATOR, APP_PATH */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief hide global variables
////////////////////////////////////////////////////////////////////////////////

  // system functions
  if (typeof SYS_DOWNLOAD !== "undefined") {
    internal.download = SYS_DOWNLOAD;
    delete SYS_DOWNLOAD;
  }
  
  if (typeof SYS_EXECUTE !== "undefined") {
    internal.executeScript = SYS_EXECUTE;
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

  if (typeof SYS_MD5 !== "undefined") {
    internal.md5 = SYS_MD5;
    delete SYS_MD5;
  }

  if (typeof SYS_GEN_RANDOM_NUMBERS !== "undefined") {
    internal.genRandomNumbers = SYS_GEN_RANDOM_NUMBERS;
    delete SYS_GEN_RANDOM_NUMBERS;
  }

  if (typeof SYS_GEN_RANDOM_ALPHA_NUMBERS !== "undefined") {
    internal.genRandomAlphaNumbers = SYS_GEN_RANDOM_ALPHA_NUMBERS;
    delete SYS_GEN_RANDOM_ALPHA_NUMBERS;
  }

  if (typeof SYS_GEN_RANDOM_SALT !== "undefined") {
    internal.genRandomSalt = SYS_GEN_RANDOM_SALT;
    delete SYS_GEN_RANDOM_SALT;
  }

  if (typeof SYS_CREATE_NONCE !== "undefined") {
    internal.createNonce = SYS_CREATE_NONCE;
    delete SYS_CREATE_NONCE;
  }

  if (typeof SYS_CHECK_AND_MARK_NONCE !== "undefined") {
    internal.checkAndMarkNonce = SYS_CHECK_AND_MARK_NONCE;
    delete SYS_CHECK_AND_MARK_NONCE;
  }

  if (typeof SYS_OUTPUT !== "undefined") {
    internal.stdOutput = SYS_OUTPUT;
    internal.output = internal.stdOutput;
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

  if (typeof SYS_RAND !== "undefined") {
    internal.rand = SYS_RAND;
    delete SYS_RAND;
  }

  if (typeof SYS_READ !== "undefined") {
    internal.read = SYS_READ;
    delete SYS_READ;
  }

  if (typeof SYS_SAVE !== "undefined") {
    internal.write = SYS_SAVE;
    delete SYS_SAVE;
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
  
  if (typeof FS_FILESIZE !== "undefined") {
    internal.fileSize = FS_FILESIZE;
    delete FS_FILESIZE;
  }
  
  if (typeof FS_MAKE_DIRECTORY !== "undefined") {
    internal.makeDirectory = FS_MAKE_DIRECTORY;
    delete FS_MAKE_DIRECTORY;
  }

  if (typeof FS_EXISTS !== "undefined") {
    internal.exists = FS_EXISTS;
    delete FS_EXISTS;
  }
  
  if (typeof FS_GET_TEMP_PATH !== "undefined") {
    internal.getTempPath = FS_GET_TEMP_PATH;
    delete FS_GET_TEMP_PATH;
  }
  
  if (typeof FS_GET_TEMP_FILE !== "undefined") {
    internal.getTempFile = FS_GET_TEMP_FILE;
    delete FS_GET_TEMP_FILE;
  }

  if (typeof FS_IS_DIRECTORY !== "undefined") {
    internal.isDirectory = FS_IS_DIRECTORY;
    delete FS_IS_DIRECTORY;
  }
  
  if (typeof FS_IS_FILE !== "undefined") {
    internal.isFile = FS_IS_FILE;
    delete FS_IS_FILE;
  }

  if (typeof FS_LIST_TREE !== "undefined") {
    internal.listTree = FS_LIST_TREE;
    delete FS_LIST_TREE;
  }

  if (typeof FS_LIST !== "undefined") {
    internal.listDirectory = FS_LIST;
    delete FS_LIST;
  }

  if (typeof FS_MOVE !== "undefined") {
    internal.move = FS_MOVE;
    delete FS_MOVE;
  }

  if (typeof FS_REMOVE !== "undefined") {
    internal.remove = FS_REMOVE;
    delete FS_REMOVE;
  }
  
  if (typeof FS_REMOVE_DIRECTORY !== "undefined") {
    internal.removeDirectory = FS_REMOVE_DIRECTORY;
    delete FS_REMOVE_DIRECTORY;
  }
  
  if (typeof FS_REMOVE_RECURSIVE_DIRECTORY !== "undefined") {
    internal.removeDirectoryRecursive = FS_REMOVE_RECURSIVE_DIRECTORY;
    delete FS_REMOVE_RECURSIVE_DIRECTORY;
  }
  
  if (typeof FS_UNZIP_FILE !== "undefined") {
    internal.unzipFile = FS_UNZIP_FILE;
    delete FS_UNZIP_FILE;
  }
  
  if (typeof FS_ZIP_FILE !== "undefined") {
    internal.zipFile = FS_ZIP_FILE;
    delete FS_ZIP_FILE;
  }

  if (typeof SYS_IMPORT_CSV_FILE !== "undefined") {
    internal.importCsvFile = SYS_IMPORT_CSV_FILE;
    delete SYS_IMPORT_CSV_FILE;
  }

  if (typeof SYS_IMPORT_JSON_FILE !== "undefined") {
    internal.importJsonFile = SYS_IMPORT_JSON_FILE;
    delete SYS_IMPORT_JSON_FILE;
  }

  if (typeof SYS_PROCESS_CSV_FILE !== "undefined") {
    internal.processCsvFile = SYS_PROCESS_CSV_FILE;
    delete SYS_PROCESS_CSV_FILE;
  }

  if (typeof SYS_PROCESS_JSON_FILE !== "undefined") {
    internal.processJsonFile = SYS_PROCESS_JSON_FILE;
    delete SYS_PROCESS_JSON_FILE;
  }

  internal.bytesSentDistribution = [];

  if (typeof BYTES_SENT_DISTRIBUTION !== "undefined") {
    internal.bytesSentDistribution = BYTES_SENT_DISTRIBUTION;
    delete BYTES_SENT_DISTRIBUTION;
  }

  internal.bytesReceivedDistribution = [];

  if (typeof BYTES_RECEIVED_DISTRIBUTION !== "undefined") {
    internal.bytesReceivedDistribution = BYTES_RECEIVED_DISTRIBUTION;
    delete BYTES_RECEIVED_DISTRIBUTION;
  }

  internal.connectionTimeDistribution = [];

  if (typeof CONNECTION_TIME_DISTRIBUTION !== "undefined") {
    internal.connectionTimeDistribution = CONNECTION_TIME_DISTRIBUTION;
    delete CONNECTION_TIME_DISTRIBUTION;
  }

  internal.requestTimeDistribution = [];

  if (typeof REQUEST_TIME_DISTRIBUTION !== "undefined") {
    internal.requestTimeDistribution = REQUEST_TIME_DISTRIBUTION;
    delete REQUEST_TIME_DISTRIBUTION;
  }

  if (typeof SYS_REQUEST_STATISTICS !== "undefined") {
    internal.requestStatistics = SYS_REQUEST_STATISTICS;
    delete SYS_REQUEST_STATISTICS;
  }

  internal.homeDirectory = "";

  if (typeof HOME !== "undefined") {
    internal.homeDirectory = HOME;
    delete HOME;
  }

  internal.developmentMode = false;

  if (typeof DEVELOPMENT_MODE !== "undefined") {
    internal.developmentMode = DEVELOPMENT_MODE;
    delete DEVELOPMENT_MODE;
  }

  if (internal.developmentMode) {
    if (typeof THREAD_NUMBER !== "undefined" && THREAD_NUMBER === 0) {
      internal.log("warning", "################################################################################");
      internal.log("warning", "development mode is active, never use this in production");
      internal.log("warning", "################################################################################");
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief modules path
////////////////////////////////////////////////////////////////////////////////

  internal.MODULES_PATH = [];

  if (typeof MODULES_PATH !== "undefined") {
    internal.MODULES_PATH = MODULES_PATH;
    delete MODULES_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief package path
////////////////////////////////////////////////////////////////////////////////

  internal.PACKAGE_PATH = [];

  if (typeof PACKAGE_PATH !== "undefined") {
    internal.PACKAGE_PATH = PACKAGE_PATH;
    delete PACKAGE_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief app path
////////////////////////////////////////////////////////////////////////////////

  internal.APP_PATH = [];

  if (typeof APP_PATH !== "undefined") {
    internal.APP_PATH = APP_PATH;
    delete APP_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief dev app path
////////////////////////////////////////////////////////////////////////////////

  internal.DEV_APP_PATH = [];

  if (typeof DEV_APP_PATH !== "undefined") {
    internal.DEV_APP_PATH = DEV_APP_PATH;
    delete DEV_APP_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet flag
////////////////////////////////////////////////////////////////////////////////

  internal.ARANGO_QUIET = false;

  if (typeof ARANGO_QUIET !== "undefined") {
    internal.ARANGO_QUIET = ARANGO_QUIET;
    delete ARANGO_QUIET;
  }

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
    [ 'COLOR_RED', 'COLOR_BOLD_RED', 'COLOR_GREEN', 'COLOR_BOLD_GREEN',
      'COLOR_BLUE', 'COLOR_BOLD_BLUE', 'COLOR_YELLOW', 'COLOR_BOLD_YELLOW', 
      'COLOR_WHITE', 'COLOR_BOLD_WHITE', 'COLOR_CYAN', 'COLOR_BOLD_CYAN', 
      'COLOR_MAGENTA', 'COLOR_BOLD_MAGENTA', 'COLOR_BLACK', 'COLOR_BOLD_BLACK', 
      'COLOR_BLINK', 'COLOR_BRIGHT', 'COLOR_RESET' ].forEach(function(color) {
        internal.COLORS[color] = '';
      });
  }

  internal.COLORS.COLOR_PUNCTUATION = internal.COLORS.COLOR_RESET;
  internal.COLORS.COLOR_STRING = internal.COLORS.COLOR_BRIGHT;
  internal.COLORS.COLOR_NUMBER = internal.COLORS.COLOR_BRIGHT;
  internal.COLORS.COLOR_INDEX = internal.COLORS.COLOR_BRIGHT;
  internal.COLORS.COLOR_TRUE = internal.COLORS.COLOR_BRIGHT;
  internal.COLORS.COLOR_FALSE = internal.COLORS.COLOR_BRIGHT;
  internal.COLORS.COLOR_NULL = internal.COLORS.COLOR_BRIGHT;
  internal.COLORS.COLOR_UNDEFINED = internal.COLORS.COLOR_BRIGHT;

  internal.NOCOLORS = { };

  var i;

  for (i in internal.COLORS) {
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
/// @brief path separator
////////////////////////////////////////////////////////////////////////////////

  internal.PATH_SEPARATOR = "/";

  if (typeof PATH_SEPARATOR !== "undefined") {
    internal.PATH_SEPARATOR = PATH_SEPARATOR;
    delete PATH_SEPARATOR;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief valgrind flag
////////////////////////////////////////////////////////////////////////////////

  internal.VALGRIND = false;

  if (typeof VALGRIND !== "undefined") {
    internal.VALGRIND = VALGRIND;
    delete VALGRIND;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief version number
////////////////////////////////////////////////////////////////////////////////

  internal.VERSION = "unknown";

  if (typeof VERSION !== "undefined") {
    internal.VERSION = VERSION;
    delete VERSION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade number
////////////////////////////////////////////////////////////////////////////////

  internal.UPGRADE = "unknown";

  if (typeof UPGRADE !== "undefined") {
    internal.UPGRADE = UPGRADE;
    delete UPGRADE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief quote cache
////////////////////////////////////////////////////////////////////////////////

  var characterQuoteCache = {
    '\b': '\\b', // ASCII 8, Backspace
    '\t': '\\t', // ASCII 9, Tab
    '\n': '\\n', // ASCII 10, Newline
    '\f': '\\f', // ASCII 12, Formfeed
    '\r': '\\r', // ASCII 13, Carriage Return
    '\"': '\\"',
    '\\': '\\\\'
  };

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

  var printArray;
  var printIndent;
  var printObject;
  var printRecursive;

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

  internal.bufferOutput = function () {
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

      internal.outputBuffer += text;
    }
  };

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
    var output = internal.output;
    var i;

    for (i = 0;  i < arguments.length;  ++i) {
      if (0 < i) {
        output(" ");
      }

      if (typeof(arguments[i]) === "string") {
        output(arguments[i]);
      }
      else {
        printRecursive(arguments[i], [], "~", [], 0);
      }
    }

    output("\n");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a single character
////////////////////////////////////////////////////////////////////////////////

  var quoteSingleJsonCharacter = function (c) {
    if (characterQuoteCache.hasOwnProperty(c)) {
      return characterQuoteCache[c];
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
    characterQuoteCache[c] = result;

    return result;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a string character
////////////////////////////////////////////////////////////////////////////////

  var quotable = /[\\\"\x00-\x1f]/g;

  var quoteJsonString = function (str) {
    return '"' + str.replace(quotable, quoteSingleJsonCharacter) + '"';
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output without a new-line
////////////////////////////////////////////////////////////////////////////////

  printRecursive = function (value, seen, path, names, level) {
    var output = internal.output;
    var p;

    if (seen === undefined) {
      seen = [];
      names = [];
    }

    p = seen.indexOf(value);

    if (0 <= p) {
      output(names[p]);
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
          printArray(value, seen, path, names, level);
        }
        else if (value.__proto__ === Object.prototype) {
          printObject(value, seen, path, names, level);
        }
        else if (typeof value.toString === "function") {
          // it's possible that toString() throws, and this looks quite ugly
          try {
            output(value.toString());
          }
          catch (e) {
          }
        }
        else {
          printObject(value, seen, path, names, level);
        }
      }
      else if (value === undefined) {
        output(internal.colors.COLOR_UNDEFINED);
        output("undefined");
        output(internal.colors.COLOR_RESET);
      }
      else {
        if (typeof(value) === "string") {
          output(internal.colors.COLOR_STRING);
          output(quoteJsonString(value));
          output(internal.colors.COLOR_RESET);
        }
        else if (typeof(value) === "boolean") {
          output(value ? internal.colors.COLOR_TRUE : internal.colors.COLOR_FALSE);
          output(String(value));
          output(internal.colors.COLOR_RESET);
        }
        else if (typeof(value) === "number") {
          output(internal.colors.COLOR_NUMBER);
          output(String(value));
          output(internal.colors.COLOR_RESET);
        }
        else if (value === null) {
          output(internal.colors.COLOR_NULL);
          output(String(value));
          output(internal.colors.COLOR_RESET);
        }
        else {
          output(String(value));
        }
      }
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

  printArray = function (object, seen, path, names, level) {
    var output = internal.output;

    if (object.length === 0) {
      output(internal.colors.COLOR_PUNCTUATION);
      output("[ ]");
      output(internal.colors.COLOR_RESET);
    }
    else {
      var i;
      var sep = " ";

      output(internal.colors.COLOR_PUNCTUATION);
      output("[");
      output(internal.colors.COLOR_RESET);

      var newLevel = level + 1;

      for (i = 0;  i < object.length;  i++) {
        output(internal.colors.COLOR_PUNCTUATION);
        output(sep);
        output(internal.colors.COLOR_RESET);

        printIndent(newLevel);

        printRecursive(object[i],
                       seen,
                       path + "[" + i + "]",
                       names,
                       newLevel);
        sep = ", ";
      }

      output(" ");

      printIndent(level);

      output(internal.colors.COLOR_PUNCTUATION);
      output("]");
      output(internal.colors.COLOR_RESET);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an object
////////////////////////////////////////////////////////////////////////////////

  printObject = function (object, seen, path, names, level) {
    var output = internal.output;
    var colors = internal.colors;
    var sep = " ";
    var k;

    output(colors.COLOR_PUNCTUATION);
    output("{");
    output(colors.COLOR_RESET);

    var newLevel = level + 1;

    for (k in object) {
      if (object.hasOwnProperty(k)) {
        var val = object[k];

        output(colors.COLOR_PUNCTUATION);
        output(sep);
        output(colors.COLOR_RESET);

        printIndent(newLevel);

        output(colors.COLOR_INDEX);
        output(quoteJsonString(k));
        output(colors.COLOR_RESET);
        output(" : ");

        printRecursive(val,
                       seen,
                       path + "[" + k + "]",
                       names,
                       newLevel);
        sep = ", ";
      }
    }

    output(" ");

    printIndent(level);

    output(colors.COLOR_PUNCTUATION);
    output("}");
    output(colors.COLOR_RESET);
  };

  internal.printObject = printObject;

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the ident for pretty printing
////////////////////////////////////////////////////////////////////////////////

  printIndent = function (level) {
    var output = internal.output;
    var j;

    if (internal.PRETTY_PRINT) {
      output("\n");

      for (j = 0; j < level; ++j) {
        output("  ");
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
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief unit-tests
////////////////////////////////////////////////////////////////////////////////

  internal.unitTests = function () {
    return SYS_UNIT_TESTS;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief unit-tests result
////////////////////////////////////////////////////////////////////////////////

  internal.setUnitTestsResult = function (value) {
    SYS_UNIT_TESTS_RESULT = value;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief global print
////////////////////////////////////////////////////////////////////////////////

  internal.print = internal.printShell;

  if (typeof internal.printBrowser === "function") {
    internal.print = internal.printBrowser;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief global printf
////////////////////////////////////////////////////////////////////////////////

  var sprintf = internal.sprintf;

  internal.printf = function () {
    internal.output(sprintf.apply(sprintf, arguments));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief start pager
////////////////////////////////////////////////////////////////////////////////

  internal.startPager = function () {};

  if (typeof SYS_START_PAGER !== "undefined") {
    internal.startPager = SYS_START_PAGER;
    delete SYS_START_PAGER;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pager
////////////////////////////////////////////////////////////////////////////////

  internal.stopPager = function () {};

  if (typeof SYS_STOP_PAGER !== "undefined") {
    internal.stopPager = SYS_STOP_PAGER;
    delete SYS_STOP_PAGER;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing
////////////////////////////////////////////////////////////////////////////////

  internal.startPrettyPrint = function (silent) {
    if (! internal.PRETTY_PRINT && ! silent) {
      internal.print("using pretty printing");
    }

    internal.PRETTY_PRINT = true;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

  internal.stopPrettyPrint = function (silent) {
    if (internal.PRETTY_PRINT && ! silent) {
      internal.print("disabled pretty printing");
    }

    internal.PRETTY_PRINT = false;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief start capture mode
////////////////////////////////////////////////////////////////////////////////

  internal.startCaptureMode = function () {
    internal.outputBuffer = "";
    internal.output = internal.bufferOutput;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stop capture mode
////////////////////////////////////////////////////////////////////////////////

  internal.stopCaptureMode = function () {
    var buffer = internal.outputBuffer;

    internal.outputBuffer = "";
    internal.output = internal.stdOutput;

    return buffer;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief start color printing
////////////////////////////////////////////////////////////////////////////////

  internal.startColorPrint = function (color, silent) {
    var schemes = {
      arangodb: {
        COLOR_PUNCTUATION: internal.COLORS.COLOR_RESET,
        COLOR_STRING: internal.COLORS.COLOR_BOLD_MAGENTA,
        COLOR_NUMBER: internal.COLORS.COLOR_BOLD_GREEN,
        COLOR_INDEX: internal.COLORS.COLOR_BOLD_CYAN,
        COLOR_TRUE: internal.COLORS.COLOR_BOLD_MAGENTA,
        COLOR_FALSE: internal.COLORS.COLOR_BOLD_MAGENTA,
        COLOR_NULL: internal.COLORS.COLOR_BOLD_YELLOW,
        COLOR_UNDEFINED: internal.COLORS.COLOR_BOLD_YELLOW
      }
    };

    if (! internal.COLOR_OUTPUT && ! silent) {
      internal.print("starting color printing");
    }

    if (color === undefined || color === null) {
      internal.colors = internal.COLORS;
    }
    else if (typeof color === "string") {
      var c;

      color = color.toLowerCase();

      if (schemes.hasOwnProperty(color)) {
        internal.colors = schemes[color];
        for (c in internal.COLORS) {
          if (internal.COLORS.hasOwnProperty(c) && ! internal.colors.hasOwnProperty(c)) {
            internal.colors[c] = internal.COLORS[c];
          }
        }
      }
      else {
        internal.colors = internal.COLORS;

        var setColor = function (key) {
          [ 'COLOR_STRING', 'COLOR_NUMBER', 'COLOR_INDEX', 'COLOR_TRUE',
            'COLOR_FALSE', 'COLOR_NULL', 'COLOR_UNDEFINED' ].forEach(function (what) {
            internal.colors[what] = internal.COLORS[key];
          });
        };

        for (c in internal.COLORS) {
          if (internal.COLORS.hasOwnProperty(c) &&
              c.replace(/^COLOR_/, '').toLowerCase() === color) {
            setColor(c);
            break;
          }
        }
      }
    }

    internal.COLOR_OUTPUT = true;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stop color printing
////////////////////////////////////////////////////////////////////////////////

  internal.stopColorPrint = function (silent) {
    if (internal.COLOR_OUTPUT && ! silent) {
      internal.print("disabled color printing");
    }

    internal.COLOR_OUTPUT = false;
    internal.colors = internal.NOCOLORS;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief debug print function
////////////////////////////////////////////////////////////////////////////////

  internal.dump = function () {
    var i;
    var oldPretty = internal.PRETTY_PRINT;
    var oldColor = internal.COLOR_OUTPUT;

    internal.startPrettyPrint(true);

    if (! oldColor) {
      internal.startColorPrint(undefined, true);
    }

    for (i = 0; i < arguments.length; ++i) {
      internal.print(arguments[i]);
    }

    if (! oldPretty) {
      internal.stopPrettyPrint(true);
    }
    if (! oldColor) {
      internal.stopColorPrint(true);
    }
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

    path = module.normalize(path);
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
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
