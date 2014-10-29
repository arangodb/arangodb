/*jshint -W051: true */
/*global require, SYS_GETLINE, SYS_LOG, jqconsole */

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
  /*jshint strict: false */
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
/// @brief group level
////////////////////////////////////////////////////////////////////////////////

  var groupLevel = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief timers
////////////////////////////////////////////////////////////////////////////////

  var timers = { };

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief internal logging
////////////////////////////////////////////////////////////////////////////////

  var log;

  try {
    // this will work when we are in arangod but not in the browser / web interface
    log = SYS_LOG;
    delete SYS_LOG;
  }
  catch (err) {
    // this will work in the web interface
    log = function (level, message) {
      if (jqconsole) {
        jqconsole.Write(message + "\n", 'jssuccess');
      }
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief internal logging with group level
////////////////////////////////////////////////////////////////////////////////

  function logGroup (level, msg) {
    'use strict';

    log(level, groupLevel + msg);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief try to prettify
////////////////////////////////////////////////////////////////////////////////

  function prepareArgs (args) {
    var ShapedJson = require("internal").ShapedJson;
    var result = [];
    var i;

    if (0 < args.length && typeof args[0] !== "string") {
      result.push("%s");
    }

    for (i = 0;  i < args.length;  ++i) {
      var arg = args[i];

      if (typeof arg === "object") {
        if (ShapedJson !== undefined && arg instanceof ShapedJson) {
          arg = inspect(arg, {prettyPrint: false});
        }
        else if (arg === null) {
          arg = "null";
        }
        else if (Object.prototype.isPrototypeOf(arg) || Array.isArray(arg)) {
          arg = inspect(arg, {prettyPrint: false});
        }
      }

      result.push(arg);
    }

    return result;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief assert
////////////////////////////////////////////////////////////////////////////////

  exports.assert = function (condition) {
    'use strict';

    if (condition) {
      return;
    }

    var args = Array.prototype.slice.call(arguments, 1);
    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(args));
    }
    catch (err) {
      msg = err + ": " + args;
    }

    logGroup("error", msg);

    require('assert').ok(condition, msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief debug
////////////////////////////////////////////////////////////////////////////////

  exports.debug = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    logGroup("debug", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief debugLines
////////////////////////////////////////////////////////////////////////////////

  exports.debugLines = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    var a = msg.split("\n");
    var i;

    for (i = 0;  i < a.length;  ++i) {
      logGroup("debug", a[i]);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief dir
////////////////////////////////////////////////////////////////////////////////

  exports.dir = function (object) {
    'use strict';

    logGroup("info", inspect(object));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief error
////////////////////////////////////////////////////////////////////////////////

  exports.error = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    logGroup("error", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief errorLines
////////////////////////////////////////////////////////////////////////////////

  exports.errorLines = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    var a = msg.split("\n");
    var i;

    for (i = 0;  i < a.length;  ++i) {
      logGroup("error", a[i]);
    }
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
////////////////////////////////////////////////////////////////////////////////

  exports.group = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    groupLevel = groupLevel + "  ";
    logGroup("info", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief groupCollapsed
////////////////////////////////////////////////////////////////////////////////

  exports.groupCollapsed = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    logGroup("info", msg);
    groupLevel = groupLevel + "  ";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief groupEnd
////////////////////////////////////////////////////////////////////////////////

  exports.groupEnd = function () {
    'use strict';

    groupLevel = groupLevel.substr(2);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief info
////////////////////////////////////////////////////////////////////////////////

  exports.info = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    logGroup("info", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief infoLines
////////////////////////////////////////////////////////////////////////////////

  exports.infoLines = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    var a = msg.split("\n");
    var i;

    for (i = 0;  i < a.length;  ++i) {
      logGroup("info", a[i]);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief log
////////////////////////////////////////////////////////////////////////////////

  exports.log = exports.info;

////////////////////////////////////////////////////////////////////////////////
/// @brief logLines
////////////////////////////////////////////////////////////////////////////////

  exports.logLines = exports.infoLines;

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

  exports.time = function (label) {
    'use strict';

    if (typeof label !== 'string') {
      throw new Error('label must be a string');
    }

    timers[label] = Date.now();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief timeEnd
////////////////////////////////////////////////////////////////////////////////

  exports.timeEnd = function(label) {
    'use strict';

    var time = timers[label];

    if (! time) {
      throw new Error('No such label: ' + label);
    }

    var duration = Date.now() - time;

    delete timers[label];

    logGroup("info", sprintf('%s: %dms', label, duration));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief trace
////////////////////////////////////////////////////////////////////////////////

  exports.trace = function () {
    var err = new Error();
    err.name = 'trace';
    err.message = sprintf.apply(sprintf, prepareArgs(arguments));
    Error.captureStackTrace(err, exports.trace);
    logGroup("info", err.stack);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief warn
////////////////////////////////////////////////////////////////////////////////

  exports.warn = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    logGroup("warning", msg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief warnLines
////////////////////////////////////////////////////////////////////////////////

  exports.warnLines = function () {
    'use strict';

    var msg;

    try {
      msg = sprintf.apply(sprintf, prepareArgs(arguments));
    }
    catch (err) {
      msg = err + ": " + arguments;
    }

    var a = msg.split("\n");
    var i;

    for (i = 0;  i < a.length;  ++i) {
      logGroup("warning", a[i]);
    }
  };

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
