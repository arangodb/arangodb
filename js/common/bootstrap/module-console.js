/*jslint indent: 2, maxlen: 120, vars: true, white: true, plusplus: true, nonpropdel: true, sloppy: true */
/*global require, SYS_GETLINE, SYS_LOG */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "console"
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
  // cannot use strict here as we are going to delete globals

  var exports = require("console");

  var sprintf = require("internal").sprintf;
  var inspect = require("internal").inspect;

// -----------------------------------------------------------------------------
// --SECTION--                                                  Module "console"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief group level
////////////////////////////////////////////////////////////////////////////////

  var groupLevel = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief timers
////////////////////////////////////////////////////////////////////////////////

  var timers = "";

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
/// @brief internal logging
////////////////////////////////////////////////////////////////////////////////

  var log = SYS_LOG;
  delete SYS_LOG;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal logging with group level
////////////////////////////////////////////////////////////////////////////////

  function logGroup (level, msg) {
    'use strict';

    log(level, groupLevel + msg);
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
/// @brief assert
/// 
/// @FUN{console.assert(@FA{expression}, @FA{format}, @FA{argument1}, ...)}
///
/// Tests that an expression is true. If not, logs a message and throws an
/// exception.
////////////////////////////////////////////////////////////////////////////////

  exports.assert = function (condition) {
    'use strict';

    if (condition) {
      return;
    }

    var args = Array.prototype.slice.call(arguments, 1);
    var msg;

    try {
      msg = sprintf.apply(sprintf, args);
    }
    catch (err) {
      msg = err + ": " + args;
    }

    log("error", msg);

    require('assert').ok(condition, msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief dir
///
/// @FUN{console.dir(@FA{object})}
///
/// Logs a static / interactive listing of all properties of the object.
////////////////////////////////////////////////////////////////////////////////

  exports.dir = function (object) {
    'use strict';

    log("info", inspect(object));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief debug
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

  exports.debug = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    log("debug", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief error
///
/// @FUN{console.error(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// error message.
////////////////////////////////////////////////////////////////////////////////

  exports.error = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    log("error", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief getline
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_GETLINE !== "undefined") {
    exports.getline = SYS_GETLINE;
    delete SYS_GETLINE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief group
///
/// @FUN{console.group(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as log
/// message. Opens a nested block to indent all future messages sent. Call
/// @FN{groupEnd} to close the block. Representation of block is up to the
/// platform, it can be an interactive block or just a set of indented sub
/// messages.
////////////////////////////////////////////////////////////////////////////////

  exports.group = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    logGroup("info", msg);

    groupLevel = groupLevel + "  ";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief groupEnd
///
/// @FUN{console.groupEnd()}
/// 
/// Closes the most recently opened block created by a call to @FN{group}.
////////////////////////////////////////////////////////////////////////////////

  exports.groupEnd = function () {
    'use strict';

    groupLevel = groupLevel.substr(2);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief info
///
/// @FUN{console.info(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// info message.
////////////////////////////////////////////////////////////////////////////////

  exports.info = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    log("info", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief log
///
/// @FUN{console.log(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// log message.
////////////////////////////////////////////////////////////////////////////////

  exports.log = exports.info;

////////////////////////////////////////////////////////////////////////////////
/// @brief time
///
/// @FUN{console.time(@FA{name})}
///
/// Creates a new timer under the given name. Call @FN{timeEnd} with the same
/// name to stop the timer and log the time elapsed.
////////////////////////////////////////////////////////////////////////////////

  exports.time = function (label) {
    'use strict';

    timers[label] = Date.now();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief timeEnd
///
/// @FUN{console.timeEnd(@FA{name})}
///
/// Stops a timer created by a call to @FN{time} and logs the time elapsed. 
////////////////////////////////////////////////////////////////////////////////

  exports.timeEnd = function(label) {
    'use strict';

    var time = timers[label];

    if (! time) {
      throw new Error('No such label: ' + label);
    }

    delete timers[label];

    var duration = Date.now() - time;
    exports.log('%s: %dms', label, duration);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief trace
/// 
/// @FUN{console.trace()}
///
/// Logs a static stack trace of JavaScript execution at the point where it is
/// called. 
////////////////////////////////////////////////////////////////////////////////

  exports.trace = function () {
    var err = new Error();
    err.name = 'trace';
    err.message = sprintf.apply(sprintf, arguments);
    Error.captureStackTrace(err, arguments.callee);
    exports.log(err.stack);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief warn
///
/// @FUN{console.warn(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to @FA{format} and logs the result as
/// warn message.
////////////////////////////////////////////////////////////////////////////////

  exports.warn = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, arguments);
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    log("warning", msg);
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
