/*jshint strict: false, unused: false, -W051: true */
/*global require, module, Module, ArangoError, SleepAndRequeue,
  CONFIGURE_ENDPOINT, REMOVE_ENDPOINT, LIST_ENDPOINTS, STARTUP_PATH,
  SYS_BASE64DECODE, SYS_BASE64ENCODE, SYS_DEBUG_SEGFAULT,
  SYS_DEBUG_CAN_USE_FAILAT, SYS_DEBUG_SET_FAILAT, SYS_DEBUG_REMOVE_FAILAT, SYS_DEBUG_CLEAR_FAILAT,
  SYS_DOWNLOAD, SYS_EXECUTE, SYS_GET_CURRENT_REQUEST, SYS_GET_CURRENT_RESPONSE,
  SYS_LOAD, SYS_LOG_LEVEL, SYS_MD5, SYS_OUTPUT, SYS_PROCESS_STATISTICS,
  SYS_RAND, SYS_SERVER_STATISTICS, SYS_SPRINTF, SYS_TIME, SYS_START_PAGER, SYS_STOP_PAGER,
  SYS_HMAC, SYS_PBKDF2, SYS_SHA512, SYS_SHA384, SYS_SHA256, SYS_SHA224, SYS_SHA1, SYS_SLEEP, SYS_WAIT,
  SYS_PARSE, SYS_PARSE_FILE, SYS_IMPORT_CSV_FILE, SYS_IMPORT_JSON_FILE, SYS_LOG,
  SYS_GEN_RANDOM_NUMBERS, SYS_GEN_RANDOM_ALPHA_NUMBERS, SYS_GEN_RANDOM_SALT, SYS_CREATE_NONCE,
  SYS_CHECK_AND_MARK_NONCE, SYS_CLIENT_STATISTICS, SYS_HTTP_STATISTICS, SYS_UNIT_TESTS, SYS_UNIT_TESTS_RESULT:true,
  SYS_PROCESS_CSV_FILE, SYS_PROCESS_JSON_FILE, ARANGO_QUIET, COLORS, COLOR_OUTPUT,
  COLOR_OUTPUT_RESET, COLOR_BRIGHT, COLOR_BLACK, COLOR_BOLD_BLACK, COLOR_BLINK, COLOR_BLUE,
  COLOR_BOLD_BLUE, COLOR_BOLD_GREEN, COLOR_RED, COLOR_BOLD_RED, COLOR_GREEN, COLOR_WHITE,
  COLOR_BOLD_WHITE, COLOR_YELLOW, COLOR_BOLD_YELLOW, COLOR_CYAN, COLOR_BOLD_CYAN, COLOR_MAGENTA,
  COLOR_BOLD_MAGENTA, PRETTY_PRINT, VALGRIND, VERSION,
  BYTES_SENT_DISTRIBUTION, BYTES_RECEIVED_DISTRIBUTION, CONNECTION_TIME_DISTRIBUTION,
  REQUEST_TIME_DISTRIBUTION, DEVELOPMENT_MODE, FE_DEVELOPMENT_MODE, THREAD_NUMBER, LOGFILE_PATH,
  SYS_PLATFORM, SYS_EXECUTE_EXTERNAL, SYS_STATUS_EXTERNAL, SYS_EXECUTE_EXTERNAL_AND_WAIT, 
  SYS_KILL_EXTERNAL, SYS_REGISTER_TASK, SYS_UNREGISTER_TASK, SYS_GET_TASK, SYS_TEST_PORT,
  SYS_IS_IP */

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

(function () {
  /*jshint strict: false */

  var exports = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError
////////////////////////////////////////////////////////////////////////////////

  if (typeof ArangoError !== "undefined") {
    exports.ArangoError = ArangoError;
    delete ArangoError;
  }
  else {
    exports.ArangoError = function (error) {
      if (error !== undefined) {
        this.error = error.error;
        this.code = error.code;
        this.errorNum = error.errorNum;
        this.errorMessage = error.errorMessage;
      }

      this.message = this.toString();
    };

    exports.ArangoError.prototype = new Error();
  }

  exports.ArangoError.prototype._PRINT = function (context) {
    context.output += this.toString();
  };

  exports.ArangoError.prototype.toString = function() {
    var errorNum = this.errorNum;
    var errorMessage = this.errorMessage || this.message;

    return "[ArangoError " + errorNum + ": " + errorMessage + "]";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief SleepAndRequeue
////////////////////////////////////////////////////////////////////////////////

  if (typeof SleepAndRequeue !== "undefined") {
    exports.SleepAndRequeue = SleepAndRequeue;
    delete SleepAndRequeue;

    exports.SleepAndRequeue.prototype._PRINT = function (context) {
      context.output += this.toString();
    };

    exports.SleepAndRequeue.prototype.toString = function() {
      return "[SleepAndRequeue sleep: " + this.sleep + "]";
  };

  }
// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief threadNumber
////////////////////////////////////////////////////////////////////////////////

  exports.threadNumber = 0;

  if (typeof THREAD_NUMBER !== "undefined") {
    exports.threadNumber = THREAD_NUMBER;
    delete THREAD_NUMBER;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief developmentMode
////////////////////////////////////////////////////////////////////////////////

  exports.developmentMode = false;

  if (typeof DEVELOPMENT_MODE !== "undefined") {
    exports.developmentMode = DEVELOPMENT_MODE;
    delete DEVELOPMENT_MODE;
  }

  if (exports.developmentMode && exports.threadNumber === 0) {
    SYS_LOG("warning", "################################################################################");
    SYS_LOG("warning", "development mode is active, never use this in production");
    SYS_LOG("warning", "################################################################################");
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief frontendDevelopmentMode
////////////////////////////////////////////////////////////////////////////////

  exports.frontendDevelopmentMode = false;

  if (typeof FE_DEVELOPMENT_MODE !== "undefined") {
    exports.frontendDevelopmentMode = FE_DEVELOPMENT_MODE;
    delete FE_DEVELOPMENT_MODE;
  }

  if (exports.frontendDevelopmentMode && exports.threadNumber === 0) {
    SYS_LOG("warning", "################################################################################");
    SYS_LOG("warning", "frontend development mode is active, never use this in production");
    SYS_LOG("warning", "################################################################################");
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief logfilePath
////////////////////////////////////////////////////////////////////////////////

  if (typeof LOGFILE_PATH !== "undefined") {
    exports.logfilePath = LOGFILE_PATH;
    delete LOGFILE_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet
////////////////////////////////////////////////////////////////////////////////

  exports.quiet = false;

  if (typeof ARANGO_QUIET !== "undefined") {
    exports.quiet = ARANGO_QUIET;
    delete ARANGO_QUIET;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief valgrind
////////////////////////////////////////////////////////////////////////////////

  exports.valgrind = false;

  if (typeof VALGRIND !== "undefined") {
    exports.valgrind = VALGRIND;
    delete VALGRIND;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief version
////////////////////////////////////////////////////////////////////////////////

  exports.version = "unknown";

  if (typeof VERSION !== "undefined") {
    exports.version = VERSION;
    delete VERSION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief platform
////////////////////////////////////////////////////////////////////////////////

  exports.platform = "unknown";

  if (typeof SYS_PLATFORM !== "undefined") {
    exports.platform = SYS_PLATFORM;
    delete SYS_PLATFORM;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief bytesSentDistribution
////////////////////////////////////////////////////////////////////////////////

  exports.bytesSentDistribution = [];

  if (typeof BYTES_SENT_DISTRIBUTION !== "undefined") {
    exports.bytesSentDistribution = BYTES_SENT_DISTRIBUTION;
    delete BYTES_SENT_DISTRIBUTION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief bytesReceivedDistribution
////////////////////////////////////////////////////////////////////////////////

  exports.bytesReceivedDistribution = [];

  if (typeof BYTES_RECEIVED_DISTRIBUTION !== "undefined") {
    exports.bytesReceivedDistribution = BYTES_RECEIVED_DISTRIBUTION;
    delete BYTES_RECEIVED_DISTRIBUTION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief connectionTimeDistribution
////////////////////////////////////////////////////////////////////////////////

  exports.connectionTimeDistribution = [];

  if (typeof CONNECTION_TIME_DISTRIBUTION !== "undefined") {
    exports.connectionTimeDistribution = CONNECTION_TIME_DISTRIBUTION;
    delete CONNECTION_TIME_DISTRIBUTION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief requestTimeDistribution
////////////////////////////////////////////////////////////////////////////////

  exports.requestTimeDistribution = [];

  if (typeof REQUEST_TIME_DISTRIBUTION !== "undefined") {
    exports.requestTimeDistribution = REQUEST_TIME_DISTRIBUTION;
    delete REQUEST_TIME_DISTRIBUTION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief startupPath
////////////////////////////////////////////////////////////////////////////////

  exports.startupPath = "";

  if (typeof STARTUP_PATH !== "undefined") {
    exports.startupPath = STARTUP_PATH;
    delete STARTUP_PATH;
  }

  if (exports.startupPath === "") {
    exports.startupPath = ".";
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief configureEndpoint
////////////////////////////////////////////////////////////////////////////////

  if (typeof CONFIGURE_ENDPOINT !== "undefined") {
    exports.configureEndpoint = CONFIGURE_ENDPOINT;
    delete CONFIGURE_ENDPOINT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief removeEndpoint
////////////////////////////////////////////////////////////////////////////////

  if (typeof REMOVE_ENDPOINT !== "undefined") {
    exports.removeEndpoint = REMOVE_ENDPOINT;
    delete REMOVE_ENDPOINT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief listEndpoints
////////////////////////////////////////////////////////////////////////////////

  if (typeof LIST_ENDPOINTS !== "undefined") {
    exports.listEndpoints = LIST_ENDPOINTS;
    delete LIST_ENDPOINTS;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Decode
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_BASE64DECODE !== "undefined") {
    exports.base64Decode = SYS_BASE64DECODE;
    delete SYS_BASE64DECODE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Encode
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_BASE64ENCODE !== "undefined") {
    exports.base64Encode = SYS_BASE64ENCODE;
    delete SYS_BASE64ENCODE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief debugSegfault
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DEBUG_SEGFAULT !== "undefined") {
    exports.debugSegfault = SYS_DEBUG_SEGFAULT;
    delete SYS_DEBUG_SEGFAULT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief debugSetFailAt
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DEBUG_SET_FAILAT !== "undefined") {
    exports.debugSetFailAt = SYS_DEBUG_SET_FAILAT;
    delete SYS_DEBUG_SET_FAILAT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief debugRemoveFailAt
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DEBUG_REMOVE_FAILAT !== "undefined") {
    exports.debugRemoveFailAt = SYS_DEBUG_REMOVE_FAILAT;
    delete SYS_DEBUG_REMOVE_FAILAT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief debugClearFailAt
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DEBUG_CLEAR_FAILAT !== "undefined") {
    exports.debugClearFailAt = SYS_DEBUG_CLEAR_FAILAT;
    delete SYS_DEBUG_CLEAR_FAILAT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief debugCanUseFailAt
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DEBUG_CAN_USE_FAILAT !== "undefined") {
    exports.debugCanUseFailAt = SYS_DEBUG_CAN_USE_FAILAT;
    delete SYS_DEBUG_CAN_USE_FAILAT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief download
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DOWNLOAD !== "undefined") {
    exports.download = SYS_DOWNLOAD;
    delete SYS_DOWNLOAD;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief executeScript
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_EXECUTE !== "undefined") {
    exports.executeScript = SYS_EXECUTE;
    delete SYS_EXECUTE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief getCurrentRequest
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_GET_CURRENT_REQUEST !== "undefined") {
    exports.getCurrentRequest = SYS_GET_CURRENT_REQUEST;
    delete SYS_GET_CURRENT_REQUEST;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief getCurrentResponse
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_GET_CURRENT_RESPONSE !== "undefined") {
    exports.getCurrentResponse = SYS_GET_CURRENT_RESPONSE;
    delete SYS_GET_CURRENT_RESPONSE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief extend
////////////////////////////////////////////////////////////////////////////////

  exports.extend = function (target, source) {
    'use strict';

    Object.getOwnPropertyNames(source)
      .forEach(function(propName) {
        Object.defineProperty(target, propName,
                              Object.getOwnPropertyDescriptor(source, propName));
      });

    return target;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief load
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_LOAD !== "undefined") {
    exports.load = SYS_LOAD;
    delete SYS_LOAD;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief logLevel
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_LOG_LEVEL !== "undefined") {
    exports.logLevel = SYS_LOG_LEVEL;
    delete SYS_LOG_LEVEL;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief md5
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_MD5 !== "undefined") {
    exports.md5 = SYS_MD5;
    delete SYS_MD5;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomNumbers
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_GEN_RANDOM_NUMBERS !== "undefined") {
    exports.genRandomNumbers = SYS_GEN_RANDOM_NUMBERS;
    delete SYS_GEN_RANDOM_NUMBERS;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomAlphaNumbers
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_GEN_RANDOM_ALPHA_NUMBERS !== "undefined") {
    exports.genRandomAlphaNumbers = SYS_GEN_RANDOM_ALPHA_NUMBERS;
    delete SYS_GEN_RANDOM_ALPHA_NUMBERS;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomSalt
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_GEN_RANDOM_SALT !== "undefined") {
    exports.genRandomSalt = SYS_GEN_RANDOM_SALT;
    delete SYS_GEN_RANDOM_SALT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief hmac
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_HMAC !== "undefined") {
    exports.hmac = SYS_HMAC;
    delete SYS_HMAC;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief pbkdf2-hmac
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_PBKDF2 !== "undefined") {
    exports.pbkdf2 = SYS_PBKDF2;
    delete SYS_PBKDF2;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief createNonce
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_CREATE_NONCE !== "undefined") {
    exports.createNonce = SYS_CREATE_NONCE;
    delete SYS_CREATE_NONCE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checkAndMarkNonce
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_CHECK_AND_MARK_NONCE !== "undefined") {
    exports.checkAndMarkNonce = SYS_CHECK_AND_MARK_NONCE;
    delete SYS_CHECK_AND_MARK_NONCE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief output
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_OUTPUT !== "undefined") {
    exports.stdOutput = SYS_OUTPUT;
    exports.output = exports.stdOutput;
    delete SYS_OUTPUT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief parse
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_PARSE !== "undefined") {
    exports.parse = SYS_PARSE;
    delete SYS_PARSE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief parseFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_PARSE_FILE !== "undefined") {
    exports.parseFile = SYS_PARSE_FILE;
    delete SYS_PARSE_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief processStatistics
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_PROCESS_STATISTICS !== "undefined") {
    exports.processStatistics = SYS_PROCESS_STATISTICS;
    delete SYS_PROCESS_STATISTICS;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief rand
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_RAND !== "undefined") {
    exports.rand = SYS_RAND;
    delete SYS_RAND;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sha512
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SHA512 !== "undefined") {
    exports.sha512 = SYS_SHA512;
    delete SYS_SHA512;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sha384
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SHA384 !== "undefined") {
    exports.sha384 = SYS_SHA384;
    delete SYS_SHA384;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sha256
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SHA256 !== "undefined") {
    exports.sha256 = SYS_SHA256;
    delete SYS_SHA256;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sha224
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SHA224 !== "undefined") {
    exports.sha224 = SYS_SHA224;
    delete SYS_SHA224;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sha1
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SHA1 !== "undefined") {
    exports.sha1 = SYS_SHA1;
    delete SYS_SHA1;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief serverStatistics
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SERVER_STATISTICS !== "undefined") {
    exports.serverStatistics = SYS_SERVER_STATISTICS;
    delete SYS_SERVER_STATISTICS;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SLEEP !== "undefined") {
    exports.sleep = SYS_SLEEP;
    delete SYS_SLEEP;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_TIME !== "undefined") {
    exports.time = SYS_TIME;
    delete SYS_TIME;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief wait
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_WAIT !== "undefined") {
    exports.wait = SYS_WAIT;
    delete SYS_WAIT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief importCsvFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_IMPORT_CSV_FILE !== "undefined") {
    exports.importCsvFile = SYS_IMPORT_CSV_FILE;
    delete SYS_IMPORT_CSV_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief importJsonFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_IMPORT_JSON_FILE !== "undefined") {
    exports.importJsonFile = SYS_IMPORT_JSON_FILE;
    delete SYS_IMPORT_JSON_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief processCsvFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_PROCESS_CSV_FILE !== "undefined") {
    exports.processCsvFile = SYS_PROCESS_CSV_FILE;
    delete SYS_PROCESS_CSV_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief processJsonFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_PROCESS_JSON_FILE !== "undefined") {
    exports.processJsonFile = SYS_PROCESS_JSON_FILE;
    delete SYS_PROCESS_JSON_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief clientStatistics
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_CLIENT_STATISTICS !== "undefined") {
    exports.clientStatistics = SYS_CLIENT_STATISTICS;
    delete SYS_CLIENT_STATISTICS;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief httpStatistics
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_HTTP_STATISTICS !== "undefined") {
    exports.httpStatistics = SYS_HTTP_STATISTICS;
    delete SYS_HTTP_STATISTICS;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief executeExternal
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_EXECUTE_EXTERNAL !== "undefined") {
    exports.executeExternal = SYS_EXECUTE_EXTERNAL;
    delete SYS_EXECUTE_EXTERNAL;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief executeExternalAndWait - instantly waits for the exit, returns 
///   joint result.
////////////////////////////////////////////////////////////////////////////////
  if (typeof SYS_EXECUTE_EXTERNAL_AND_WAIT !== "undefined") {
    exports.executeExternalAndWait = SYS_EXECUTE_EXTERNAL_AND_WAIT;
    delete SYS_EXECUTE_EXTERNAL_AND_WAIT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief killExternal
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_KILL_EXTERNAL !== "undefined") {
    exports.killExternal = SYS_KILL_EXTERNAL;
    delete SYS_KILL_EXTERNAL;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief statusExternal
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_STATUS_EXTERNAL !== "undefined") {
    exports.statusExternal = SYS_STATUS_EXTERNAL;
    delete SYS_STATUS_EXTERNAL;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief registerTask
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_REGISTER_TASK !== "undefined") {
    exports.registerTask = SYS_REGISTER_TASK;
    delete SYS_REGISTER_TASK;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisterTask
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_UNREGISTER_TASK !== "undefined") {
    exports.unregisterTask = SYS_UNREGISTER_TASK;
    delete SYS_UNREGISTER_TASK;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief getTasks
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_GET_TASK !== "undefined") {
    exports.getTask = SYS_GET_TASK;
    delete SYS_GET_TASK;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief testPort
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_TEST_PORT !== "undefined") {
    exports.testPort = SYS_TEST_PORT;
    delete SYS_TEST_PORT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief isIP
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_IS_IP !== "undefined") {
    exports.isIP = SYS_IS_IP;
    delete SYS_IS_IP;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief unitTests
////////////////////////////////////////////////////////////////////////////////

  exports.unitTests = function () {
    'use strict';

    return SYS_UNIT_TESTS;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief setUnitTestsResult
////////////////////////////////////////////////////////////////////////////////

  exports.setUnitTestsResult = function (value) {
    // do not use strict here
    SYS_UNIT_TESTS_RESULT = value;
  };

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                          PRINTING
// -----------------------------------------------------------------------------

(function () {
  // cannot use strict here as we are going to delete globals

  var exports = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                         public printing variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief COLORS
////////////////////////////////////////////////////////////////////////////////

  exports.COLORS = {};

  if (typeof COLORS !== "undefined") {
    exports.COLORS = COLORS;
    delete COLORS;
  }
  else {
    [ 'COLOR_RED', 'COLOR_BOLD_RED', 'COLOR_GREEN', 'COLOR_BOLD_GREEN',
      'COLOR_BLUE', 'COLOR_BOLD_BLUE', 'COLOR_YELLOW', 'COLOR_BOLD_YELLOW',
      'COLOR_WHITE', 'COLOR_BOLD_WHITE', 'COLOR_CYAN', 'COLOR_BOLD_CYAN',
      'COLOR_MAGENTA', 'COLOR_BOLD_MAGENTA', 'COLOR_BLACK', 'COLOR_BOLD_BLACK',
      'COLOR_BLINK', 'COLOR_BRIGHT', 'COLOR_RESET' ].forEach(function(color) {
        exports.COLORS[color] = '';
      });
  }

  exports.COLORS.COLOR_PUNCTUATION = exports.COLORS.COLOR_RESET;
  exports.COLORS.COLOR_STRING = exports.COLORS.COLOR_BRIGHT;
  exports.COLORS.COLOR_NUMBER = exports.COLORS.COLOR_BRIGHT;
  exports.COLORS.COLOR_INDEX = exports.COLORS.COLOR_BRIGHT;
  exports.COLORS.COLOR_TRUE = exports.COLORS.COLOR_BRIGHT;
  exports.COLORS.COLOR_FALSE = exports.COLORS.COLOR_BRIGHT;
  exports.COLORS.COLOR_NULL = exports.COLORS.COLOR_BRIGHT;
  exports.COLORS.COLOR_UNDEFINED = exports.COLORS.COLOR_BRIGHT;

// -----------------------------------------------------------------------------
// --SECTION--                                        private printing variables
// -----------------------------------------------------------------------------

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
/// @brief colors
////////////////////////////////////////////////////////////////////////////////

  var colors = exports.COLORS;

////////////////////////////////////////////////////////////////////////////////
/// @brief useColor
////////////////////////////////////////////////////////////////////////////////

  var useColor = false;

  if (typeof COLOR_OUTPUT !== "undefined") {
    useColor = COLOR_OUTPUT;
    delete COLOR_OUTPUT;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief usePrettyPrint
////////////////////////////////////////////////////////////////////////////////

  var usePrettyPrint = false;

  if (typeof PRETTY_PRINT !== "undefined") {
    usePrettyPrint = PRETTY_PRINT;
    delete PRETTY_PRINT;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                        private printing functions
// -----------------------------------------------------------------------------

  var printRecursive;

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a single character
////////////////////////////////////////////////////////////////////////////////

  function quoteSingleJsonCharacter (c) {
    'use strict';

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
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a string character
////////////////////////////////////////////////////////////////////////////////

  var quotable = /[\\\"\x00-\x1f]/g;

  function quoteJsonString (str) {
    'use strict';

    return '"' + str.replace(quotable, quoteSingleJsonCharacter) + '"';
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the ident for pretty printing
////////////////////////////////////////////////////////////////////////////////

  function printIndent (context) {
    'use strict';

    var j;
    var indent = "";

    if (context.prettyPrint) {
      indent += "\n";

      for (j = 0; j < context.level; ++j) {
        indent += "  ";
      }
    }

    context.output += indent;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

  function printArray (object, context) {
    'use strict';

    var useColor = context.useColor;

    if (object.length === 0) {
      if (useColor) {
        context.output += colors.COLOR_PUNCTUATION;
      }

      context.output += "[ ]";

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else {
      var i;

      if (useColor) {
        context.output += colors.COLOR_PUNCTUATION;
      }

      context.output += "[";

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }

      var newLevel = context.level + 1;
      var sep = " ";

      context.level = newLevel;

      for (i = 0;  i < object.length;  i++) {
        if (useColor) {
          context.output += colors.COLOR_PUNCTUATION;
        }

        context.output += sep;

        if (useColor) {
          context.output += colors.COLOR_RESET;
        }

        printIndent(context);

        var path = context.path;
        context.path += "[" + i + "]";

        printRecursive(object[i], context);

        if (context.emit && context.output.length >= context.emit) {
          exports.output(context.output);
          context.output = "";
        }

        context.path = path;
        sep = ", ";
      }

      context.level = newLevel - 1;
      context.output += " ";

      printIndent(context);

      if (useColor) {
        context.output += colors.COLOR_PUNCTUATION;
      }

      context.output += "]";

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an object
////////////////////////////////////////////////////////////////////////////////

  function printObject (object, context) {
    'use strict';

    var useColor = context.useColor;
    var sep = " ";

    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += "{";

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }

    var newLevel = context.level + 1;

    context.level = newLevel;

    var keys = Object.keys(object);
    var i, n = keys.length;

    for (i = 0; i < n; ++i) {
      var k = keys[i];
      var val = object[k];

      if (useColor) {
        context.output += colors.COLOR_PUNCTUATION;
      }

      context.output += sep;

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }

      printIndent(context);

      if (useColor) {
        context.output += colors.COLOR_INDEX;
      }

      context.output += quoteJsonString(k);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }

      context.output += " : ";

      var path = context.path;
      context.path += "[" + k + "]";

      printRecursive(val, context);

      context.path = path;
      sep = ", ";

      if (context.emit && context.output.length >= context.emit) {
        exports.output(context.output);
        context.output = "";
      }
    }

    context.level = newLevel - 1;
    context.output += " ";

    printIndent(context);

    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += "}";

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output without a new-line
////////////////////////////////////////////////////////////////////////////////

  var funcRE = /function ([^\(]*)?\(\) \{ \[native code\] \}/;
  var func2RE = /function ([^\(]*)?\((.*)\) \{/;

  exports.printRecursive = printRecursive = function (value, context) {
    'use strict';

    var useColor = context.useColor;
    var customInspect = context.customInspect;
    var useToString = context.useToString;
    var limitString = context.limitString;
    var showFunction = context.showFunction;

    if (typeof context.seen === "undefined") {
      context.seen = [];
      context.names = [];
    }

    var p = context.seen.indexOf(value);

    if (0 <= p) {
      context.output += context.names[p];
    }
    else {
      if (value && (value instanceof Object || (typeof value === 'object' && Object.getPrototypeOf(value) === null))) {
        context.seen.push(value);
        context.names.push(context.path);
        if (customInspect && typeof value._PRINT === "function") {
          value._PRINT(context);

          if (context.emit && context.output.length >= context.emit) {
            exports.output(context.output);
            context.output = "";
          }
        }
        else if (value instanceof Array) {
          printArray(value, context);
        }
        else if (
          value.toString === Object.prototype.toString
          || (typeof value === 'object' && Object.getPrototypeOf(value) === null)
        ) {
          printObject(value, context);

          if (context.emit && context.output.length >= context.emit) {
            exports.output(context.output);
            context.output = "";
          }
        }
        else if (typeof value === "function") {

          // it's possible that toString() throws, and this looks quite ugly
          try {
            var s = value.toString();

            if (0 < context.level && ! showFunction) {
              var a = s.split("\n");
              var f = a[0];

              var m = funcRE.exec(f);

              if (m !== null) {
                if (m[1] === undefined) {
                  context.output += 'function {native code}';
                }
                else {
                  context.output += 'function ' + m[1] + ' {native code}';
                }
              }
              else {
                m = func2RE.exec(f);

                if (m !== null) {
                  if (m[1] === undefined) {
                    context.output += 'function ' + '(' + m[2] +') { ... }';
                  }
                  else {
                    context.output += 'function ' + m[1] + ' (' + m[2] +') { ... }';
                  }
                }
                else {
                  f = f.substr(8, f.length - 10).trim();
                  context.output += '[Function "' + f + '" ...]';
                }
              }
            }
            else {
              context.output += s;
            }
          }
          catch (e1) {
            exports.stdOutput(String(e1));
            context.output += "[Function]";
          }
        }
        else if (useToString && typeof value.toString === "function") {
          try {
            context.output += value.toString();
          }
          catch (e2) {
            context.output += "[Object ";
            printObject(value, context);
            context.output += "]";
          }
        }
        else {
          context.output += "[Object ";
          printObject(value, context);
          context.output += "]";
        }
      }
      else if (value === undefined) {
        if (useColor) {
          context.output += colors.COLOR_UNDEFINED;
        }

        context.output += "undefined";

        if (useColor) {
          context.output += colors.COLOR_RESET;
        }
      }
      else if (typeof(value) === "string") {
        if (useColor) {
          context.output += colors.COLOR_STRING;
        }

        if (limitString) {
          if (limitString < value.length) {
            value = value.substr(0, limitString) + "...";
          }
        }

        context.output += quoteJsonString(value);

        if (useColor) {
          context.output += colors.COLOR_RESET;
        }
      }
      else if (typeof(value) === "boolean") {
        if (useColor) {
          context.output += value ? colors.COLOR_TRUE : colors.COLOR_FALSE;
        }

        context.output += String(value);

        if (useColor) {
          context.output += colors.COLOR_RESET;
        }
      }
      else if (typeof(value) === "number") {
        if (useColor) {
          context.output += colors.COLOR_NUMBER;
        }

        context.output += String(value);

        if (useColor) {
          context.output += colors.COLOR_RESET;
        }
      }
      else if (value === null) {
        if (useColor) {
          context.output += colors.COLOR_NULL;
        }

        context.output += String(value);

        if (useColor) {
          context.output += colors.COLOR_RESET;
        }
      }
      else if (typeof(value) === "symbol") {
        if (useColor) {
          context.output += colors.COLOR_NULL;
        }

        context.output += value.toString();

        if (useColor) {
          context.output += colors.COLOR_RESET;
        }
      }
      else {
        context.output += String(value);
      }
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief buffers output instead of printing it
////////////////////////////////////////////////////////////////////////////////

  function bufferOutput () {
    'use strict';

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

      exports.outputBuffer += text;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all arguments
///
/// @FUN{exports.printShell(@FA{arg1}, @FA{arg2}, @FA{arg3}, ...)}
///
/// Only available in shell mode.
///
/// Prints the arguments. If an argument is an object having a function
/// @FN{_PRINT}, then this function is called. A final newline is printed.
////////////////////////////////////////////////////////////////////////////////

  function printShell () {
    'use strict';

    var output = exports.output;
    var i;

    for (i = 0;  i < arguments.length;  ++i) {
      if (0 < i) {
        output(" ");
      }

      if (typeof(arguments[i]) === "string") {
        output(arguments[i]);
      }
      else {
        var context = {
          customInspect: true,
          emit: 16384,
          level: 0,
          limitString: 80,
          names: [],
          output: "",
          path: "~",
          prettyPrint: usePrettyPrint,
          seen: [],
          showFunction: false,
          useColor: useColor,
          useToString: true
        };

        printRecursive(arguments[i], context);

        output(context.output);
      }
    }

    output("\n");
  }

// -----------------------------------------------------------------------------
// --SECTION--                                         public printing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief flatten
////////////////////////////////////////////////////////////////////////////////

  var hasOwnProperty = Function.prototype.call.bind(Object.prototype.hasOwnProperty);

  exports.flatten = function (obj, seen) {
    'use strict';

    if (!obj || (typeof obj !== 'object' && typeof obj !== 'function')) {
      return obj;
    }

    if (obj instanceof Date) {
      return obj.toJSON();
    }

    if (!seen) {
      seen = [];
    }

    var result = Object.create(null),
      src = obj,
      keys,
      i,
      key,
      val;

    if (typeof obj === 'function') {
      result.__exec = String(obj);
    }

    while (src) {
      if (
        seen.indexOf(src) !== -1
          || (obj.constructor && src === obj.constructor.prototype)
      ) {
        break;
      }
      seen.push(src);
      keys = Object.getOwnPropertyNames(src);
      for (i = 0; i < keys.length; i++) {
        key = keys[i];
        if (typeof src !== 'function' || (
          key !== 'arguments' && key !== 'caller' && key !== 'callee'
        )) {
          if (key.charAt(0) !== '_' && !hasOwnProperty(result, key)) {
            val = obj[key];
            if (seen.indexOf(val) !== -1 && (
              typeof val === 'object' || typeof val === 'function'
            )) {
              result[key] = '[Circular]';
            } else {
              result[key] = exports.flatten(val, seen);
            }
          }
        }
      }
      src = Object.getPrototypeOf(src);
    }

    if (obj.constructor && obj.constructor.name) {
      if (obj instanceof Error && obj.name === Error.name) {
        result.name = obj.constructor.name;
      } else if (!hasOwnProperty(result, 'constructor')) {
        result.constructor = {name: obj.constructor.name};
      }
    }

    return result;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief inspect
////////////////////////////////////////////////////////////////////////////////

  exports.inspect = function (object, options) {
    'use strict';

    var context = {
      customInspect: options && options.customInspect,
      emit: false,
      level: 0,
      limitString: false,
      names: [],
      output: "",
      prettyPrint: true,
      path: "~",
      seen: [],
      showFunction: true,
      useColor: false,
      useToString: false
    };

    if (options && options.hasOwnProperty("prettyPrint")) {
      context.prettyPrint = options.prettyPrint;
    }

    printRecursive(object, context);

    return context.output;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief sprintf
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SPRINTF !== "undefined") {
    exports.sprintf = SYS_SPRINTF;
    delete SYS_SPRINTF;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief printf
////////////////////////////////////////////////////////////////////////////////

  var sprintf = exports.sprintf;

  exports.printf = function () {
    'use strict';

    exports.output(sprintf.apply(sprintf, arguments));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

  if (typeof exports.printBrowser === "function") {
    exports.printShell = printShell;
    exports.print = exports.printBrowser;
  }
  else {
    exports.print = printShell;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief printObject
////////////////////////////////////////////////////////////////////////////////

  exports.printObject = printObject;

////////////////////////////////////////////////////////////////////////////////
/// @brief startCaptureMode
////////////////////////////////////////////////////////////////////////////////

  exports.startCaptureMode = function () {
    'use strict';

    exports.outputBuffer = "";
    exports.output = bufferOutput;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stopCaptureMode
////////////////////////////////////////////////////////////////////////////////

  exports.stopCaptureMode = function () {
    'use strict';

    var buffer = exports.outputBuffer;

    exports.outputBuffer = "";
    exports.output = exports.stdOutput;

    return buffer;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief startPager
////////////////////////////////////////////////////////////////////////////////

  exports.startPager = function () {};

  if (typeof SYS_START_PAGER !== "undefined") {
    exports.startPager = SYS_START_PAGER;
    delete SYS_START_PAGER;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief stopPager
////////////////////////////////////////////////////////////////////////////////

  exports.stopPager = function () {};

  if (typeof SYS_STOP_PAGER !== "undefined") {
    exports.stopPager = SYS_STOP_PAGER;
    delete SYS_STOP_PAGER;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief startPrettyPrint
////////////////////////////////////////////////////////////////////////////////

  exports.startPrettyPrint = function (silent) {
    'use strict';

    if (! usePrettyPrint && ! silent) {
      exports.print("using pretty printing");
    }

    usePrettyPrint = true;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stopPrettyPrint
////////////////////////////////////////////////////////////////////////////////

  exports.stopPrettyPrint = function (silent) {
    'use strict';

    if (usePrettyPrint && ! silent) {
      exports.print("disabled pretty printing");
    }

    usePrettyPrint = false;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief startColorPrint
////////////////////////////////////////////////////////////////////////////////

  exports.startColorPrint = function (color, silent) {
    'use strict';

    var schemes = {
      arangodb: {
        COLOR_PUNCTUATION: exports.COLORS.COLOR_RESET,
        COLOR_STRING: exports.COLORS.COLOR_BOLD_MAGENTA,
        COLOR_NUMBER: exports.COLORS.COLOR_BOLD_GREEN,
        COLOR_INDEX: exports.COLORS.COLOR_BOLD_CYAN,
        COLOR_TRUE: exports.COLORS.COLOR_BOLD_MAGENTA,
        COLOR_FALSE: exports.COLORS.COLOR_BOLD_MAGENTA,
        COLOR_NULL: exports.COLORS.COLOR_BOLD_YELLOW,
        COLOR_UNDEFINED: exports.COLORS.COLOR_BOLD_YELLOW
      }
    };

    if (! useColor && ! silent) {
      exports.print("starting color printing");
    }

    if (color === undefined || color === null) {
      color = null;
    }
    else if (typeof color === "string") {
      var c;

      color = color.toLowerCase();

      if (schemes.hasOwnProperty(color)) {
        colors = schemes[color];

        for (c in exports.COLORS) {
          if (exports.COLORS.hasOwnProperty(c) && ! colors.hasOwnProperty(c)) {
            colors[c] = exports.COLORS[c];
          }
        }
      }
      else {
        colors = exports.COLORS;

        var setColor = function (key) {
          [ 'COLOR_STRING', 'COLOR_NUMBER', 'COLOR_INDEX', 'COLOR_TRUE',
            'COLOR_FALSE', 'COLOR_NULL', 'COLOR_UNDEFINED' ].forEach(function (what) {
            colors[what] = exports.COLORS[key];
          });
        };

        for (c in exports.COLORS) {
          if (exports.COLORS.hasOwnProperty(c) &&
              c.replace(/^COLOR_/, '').toLowerCase() === color) {
            setColor(c);
            break;
          }
        }
      }
    }

    useColor = true;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief stopColorPrint
////////////////////////////////////////////////////////////////////////////////

  exports.stopColorPrint = function (silent) {
    'use strict';

    if (useColor && ! silent) {
      exports.print("disabled color printing");
    }

    useColor = false;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                          public utility functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief exponentialBackoff
////////////////////////////////////////////////////////////////////////////////

  exports.exponentialBackOff = function (n, i) {
    'use strict';
    if (i === 0) {
      return 0;
    }
    if (n === 0) {
      return 0;
    }
    if (n === 1) {
      return Math.random() < 0.5 ? 0 : i;
    }
    return Math.floor(Math.random() * (n + 1)) * i;
  };

}());

// -----------------------------------------------------------------------------
// --SECTION--                                         global printing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

function print () {
  'use strict';

  var internal = require("internal");
  internal.print.apply(internal.print, arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief printf
////////////////////////////////////////////////////////////////////////////////

function printf () {
  'use strict';

  var internal = require("internal");
  internal.printf.apply(internal.printf, arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print_plain
////////////////////////////////////////////////////////////////////////////////

function print_plain () {
  'use strict';

  var output = require("internal").output;
  var printRecursive = require("internal").printRecursive;
  var i;

  for (i = 0;  i < arguments.length;  ++i) {
    if (0 < i) {
      output(" ");
    }

    if (typeof(arguments[i]) === "string") {
      output(arguments[i]);
    }
    else {
      var context = {
        names: [],
        seen: [],
        path: "~",
        level: 0,
        output: "",
        prettyPrint: false,
        useColor: false,
        customInspect: true
      };

      printRecursive(arguments[i], context);

      output(context.output);
    }
  }

  output("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start_pretty_print
////////////////////////////////////////////////////////////////////////////////

function start_pretty_print () {
  'use strict';

  require("internal").startPrettyPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop_pretty_print
////////////////////////////////////////////////////////////////////////////////

function stop_pretty_print () {
  'use strict';

  require("internal").stopPrettyPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start_color_print
////////////////////////////////////////////////////////////////////////////////

function start_color_print (color) {
  'use strict';

  require("internal").startColorPrint(color, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop_color_print
////////////////////////////////////////////////////////////////////////////////

function stop_color_print () {
  'use strict';

  require("internal").stopColorPrint();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
