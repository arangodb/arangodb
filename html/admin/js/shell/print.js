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
    if (internal.characterQuoteCache.hasOwnProperty(c)) {
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

  var quoteJsonString = function (str) {
    var quotable = /[\\\"\x00-\x1f]/g;
    return '"' + str.replace(quotable, internal.quoteSingleJsonCharacter) + '"';
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
  
  internal.printRecursive = printRecursive;

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an array
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
  
  internal.printArray = printArray;

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
///
/// @FUN{internal.printIndent(@FA{level})}
///
/// Only available in shell mode.
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
  
  internal.printIndent = printIndent;

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
