/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global require, print */

////////////////////////////////////////////////////////////////////////////////
/// @brief printing
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
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

    if (internal.COLOR_OUTPUT) {
      internal.output(internal.COLOR_OUTPUT_DEFAULT);
      internal.output(internal.COLOR_OUTPUT_RESET);
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
        if ('_PRINT' in value) {
          value._PRINT(seen, path, names, level);
        }
        else if (value instanceof Array) {
          internal.printArray(value, seen, path, names, level);
        }
        else if (value.__proto__ === Object.prototype) {
          internal.printObject(value, seen, path, names, level);
        }
        else if ('toString' in value) {
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
          internal.output(internal.quoteJsonString(value));
        }
        else {
          internal.output(String(value));
        }
      }
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the ident for pretty printing
///
/// @FUN{internal.printIndent(@FA{level})}
///
/// Only available in shell mode.
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
/// @brief JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

  internal.printArray = function (object, seen, path, names, level) {
    if (object.length === 0) {
      internal.output("[ ]");
    }
    else {
      var i;
      var sep = "";

      internal.output("[");

      var newLevel = level + 1;

      for (i = 0;  i < object.length;  i++) {
        internal.output(sep);

        internal.printIndent(newLevel);

        internal.printRecursive(object[i],
                                seen,
                                path + "[" + i + "]",
                                names,
                                newLevel);
        sep = ", ";
      }

      internal.printIndent(level);

      internal.output("]");
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an object
////////////////////////////////////////////////////////////////////////////////

  internal.printObject = function (object, seen, path, names, level) {
    var sep = " ";
    var k;

    internal.output("{");

    var newLevel = level + 1;

    for (k in object) {
      if (object.hasOwnProperty(k)) {
        var val = object[k];

        internal.output(sep);

        internal.printIndent(newLevel);

        if (internal.COLOR_OUTPUT) {
          internal.output(internal.COLOR_OUTPUT_DEFAULT,
                          k,
                          internal.COLOR_OUTPUT_RESET, 
                          " : ");
        }
        else {
          internal.output(internal.quoteJsonString(k), " : ");
        }

        internal.printRecursive(val,
                                seen,
                                path + "[" + k + "]",
                                names,
                                newLevel);
        sep = ", ";
      }
    }

    internal.printIndent(level);

    internal.output("}");
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                  global functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global print
////////////////////////////////////////////////////////////////////////////////

// must be a variable definition for the browser
if (typeof require("internal").printBrowser === "function") {
  print = require("internal").print = require("internal").printBrowser;
}
else {
  print = require("internal").print = require("internal").printShell;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
