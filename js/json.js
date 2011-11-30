////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript JSON utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
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
