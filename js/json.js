////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript JSON utility functions
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
// --SECTION--                                                            toJson
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json V8 JSON
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an object
///
/// @FUN{toJson(@FA{object}, @FA{useNewLine})}
///
/// The representation of a JSON object as string. If @FA{useNewLine} is true,
/// then new-lines are used to format the output in a nice way.
////////////////////////////////////////////////////////////////////////////////

function toJson (x, useNL, indent) {
  if (x === null) {
    return "null";
  }

  if (x === undefined) {
    return "undefined";
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
    return x.toJson(useNL, indent);
  }

  return "" + x;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

Array.prototype.toJson = function(useNL, indent) {
  var nl = useNL ? "\n" : " ";

  if (! indent) {
    indent = "";
  }

  if (! useNL) {
    indent = "";
  }

  if (this.length == 0) {
    return indent + "[]";
  }

  var s = "[" + nl;
  var oldIndent = indent;

  if (useNL) {
    indent += "  ";
  }

  for (var i = 0;  i < this.length;  i++) {
    s += indent + toJson(this[i], useNL, indent);

    if (i < this.length - 1) {
      s += "," + nl;
    }
  }

  s += nl + oldIndent + "]";

  return s;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an object
////////////////////////////////////////////////////////////////////////////////

Object.prototype.toJson = function(useNL, indent) {
  var nl = useNL ? "\n" : " ";

  if (! indent) {
    indent = "";
  }

  var s = "{" + nl;

  // push one level of indent
  var oldIndent = indent;
  indent += "  ";

  if (! useNL) {
    indent = "";
  }

  var sep = "";

  for (var k in this) {
    if (this.hasOwnProperty(k)) {
      var val = this[k];

      s += sep + indent + "\"" + k + "\" : " + toJson(val, useNL, indent);
      sep = "," + nl;
    }
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
