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

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                          printing
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output
///
/// @FUN{print(@FA{arg1}, @FA{arg2}, @FA{arg3}, ...)}
///
/// Only available in shell mode.
///
/// Prints the arguments. If an argument is an object having a
/// function @FN{PRINT}, then this function is called. Otherwise @FN{toJson} is
/// used.  A final newline is printed
///
/// @verbinclude fluent40
////////////////////////////////////////////////////////////////////////////////

function print () {
  for (var i = 0;  i < arguments.length;  ++i) {
    if (0 < i) {
      internal.output(" ");
    }

    PRINT(arguments[i], [], "~", []);
  }

  internal.output("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output without a new-line
////////////////////////////////////////////////////////////////////////////////

function PRINT (value, seen, path, names) {
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
    seen.push(value);
    names.push(path);

    if (value instanceof Object) {
      if ('PRINT' in value) {
        value.PRINT(seen, path, names);
      }
      else if (value.__proto__ === Object.prototype) {
        PRINT_OBJECT(value, seen, path, names);
      }
      else if ('toString' in value) {
        internal.output(value.toString());
      }
      else {
        PRINT_OBJECT(value, seen, path, names);
      }
    }
    else if (value === undefined) {
      internal.output("undefined");
    }
    else {
      internal.output("" + value);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             Array
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

Array.prototype.PRINT = function(seen, path, names) {
  if (this.length == 0) {
    internal.output("[ ]");
  }
  else {
    var sep = " ";

    internal.output("[");

    for (var i = 0;  i < this.length;  i++) {
      internal.output(sep);
      PRINT(this[i], seen, path + "[" + i + "]", names);
      sep = ", ";
    }

    internal.output(" ]");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          Function
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a function
////////////////////////////////////////////////////////////////////////////////

Function.prototype.PRINT = function() {
  internal.output(this.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            Object
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief string representation of an object
////////////////////////////////////////////////////////////////////////////////

function PRINT_OBJECT (object, seen, path, names) {
  var sep = " ";

  internal.output("{");

  for (var k in object) {
    if (object.hasOwnProperty(k)) {
      var val = object[k];

      internal.output(sep, k, " : ");
      PRINT(val, seen, path + "[" + k + "]", names);
      sep = ", ";
    }
  }

  internal.output(" }");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                AvocadoFluentQuery
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global variable holding the current printed query
////////////////////////////////////////////////////////////////////////////////

var it = undefined;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of results to print
////////////////////////////////////////////////////////////////////////////////

var queryLimit = 20;

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a query
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery.prototype.PRINT = function() {
  if (this instanceof AvocadoFluentQuery) {
    var count = 0;

    try {
      while (this.hasNext() && count++ < queryLimit) {
        internal.output(JSON.stringify(this.next()), "\n");
      }

      if (this.hasNext()) {
        internal.output("...more results...");
      }

      it = this;
    }
    catch (e) {
      internal.output("encountered error while printing: " + e);
    }
  }
  else {
    internal.output(this.toString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a query
////////////////////////////////////////////////////////////////////////////////

AvocadoCursor.prototype.PRINT = function() {
  if (this instanceof AvocadoCursor) {
    var count = 0;

    try {
      while (this.hasNext() && count++ < queryLimit) {
        internal.output(JSON.stringify(this.next()), "\n");
      }

      if (this.hasNext()) {
        internal.output("...more results...");
      }

      it = this;
    }
    catch (e) {
      internal.output("encountered error while printing: " + e);
    }
  }
  else {
    internal.output(this.toString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
