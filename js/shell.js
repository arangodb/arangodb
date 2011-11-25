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
// --SECTION--                                                  query evaluation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell V8 Shell
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

AvocadoQuery.prototype.print = function(q) {
  var count = 0;

  try {
    while (q.hasNext() && count++ < queryLimit) {
      output(toJson(q.next()), "\n");
    }

    if (q.hasNext()) {
      output("...more results...");
    }

    it = q;
  }
  catch (e) {
    output("encountered error while printing: " + e);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            toJson
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell V8 Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an object
////////////////////////////////////////////////////////////////////////////////

toJson = function(x, indent , useNL) {
  if (x === null) {
    return "null";
  }

  if (x === undefined) {
    return "undefined";
  }

  if (useNL === undefined && (indent === true || indent === false)) {
    useNL = indent;
    indent = "";
  }

  if (! indent) {
    indent = "";
  }

  if (x instanceof String || typeof x === "string") {
    var s = "\"";

    for (var i = 0;  i < x.length;  i++){
      switch (x[i]) {
        case '"': s += '\\"'; break;
        case '\\': s += '\\\\'; break;
        case '\b': s += '\\b'; break;
        case '\f': s += '\\f'; break;
        case '\n': s += '\\n'; break;
        case '\r': s += '\\r'; break;
        case '\t': s += '\\t'; break;

        default: {
          var code = x.charCodeAt(i);

          if (code < 0x20) {
            s += (code < 0x10 ? '\\u000' : '\\u00') + code.toString(16);
          }
          else {
            s += x[i];
          }
        }
      }
    }

    return s + "\"";
  }

  if (x instanceof Number || typeof x === "number") {
    return "" + x;
  }

  if (x instanceof Boolean || typeof x === "boolean") {
    return "" + x;
  }

  if (x instanceof Function) {
    return x.toString();
  }

  if (x instanceof Object) {
    return toJsonObject(x, indent , useNL);
  }

  return "" + x;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

Array.toJson = function(a, indent, useNL) {
  var nl = useNL ? "\n" : " ";

  if (! indent) {
    indent = "";
  }

  if (! useNL) {
    indent = "";
  }

  if (a.length == 0) {
    return indent + "[]";
  }

  var s = "[" + nl;
  var oldIndent = indent;

  if (useNL) {
    indent += "  ";
  }

  for (var i = 0;  i < a.length;  i++) {
    s += indent + toJson(a[i], indent , useNL);

    if (i < a.length - 1) {
      s += "," + nl;
    }
  }

  s += nl + oldIndent + "]";

  return s;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an object
////////////////////////////////////////////////////////////////////////////////

toJsonObject = function(x, indent , useNL) {
  var nl = useNL ? "\n" : " ";

  if (! indent) {
    indent = "";
  }

  if (typeof(x.toJson) == "function" && x.toJson != toJson) {
    return indent + x.toJson(indent, useNL);
  }

  if (x.constructor && typeof(x.constructor.toJson) == "function" && x.constructor.toJson != toJson) {
    return x.constructor.toJson(x, indent , useNL);
  }

  var s = "{" + nl;

  // push one level of indent
  var oldIndent = indent;
  indent += "  ";

  if (! useNL) {
    indent = "";
  }

  var sep = "";

  for (var k in x) {
    var val = x[k];

    s += sep + indent + "\"" + k + "\" : " + toJson(val, indent , useNL);
    sep = "," + nl;
  }

  // pop one level of indent
  indent = oldIndent;

  return s + nl + indent + "}";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
