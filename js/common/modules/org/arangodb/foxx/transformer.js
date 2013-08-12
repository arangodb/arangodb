/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var Transformer,
  transform,
  ArrayIterator,
  extend = require("underscore").extend;

ArrayIterator = function (arr) {
  this.array = arr;
  this.currentLineNumber = -1;
};

extend(ArrayIterator.prototype, {
  next: function () {
    this.currentLineNumber += 1;
    return this.array[this.currentLineNumber];
  },

  current: function () {
    return this.array[this.currentLineNumber];
  },

  hasNext: function () {
    return this.currentLineNumber < (this.array.length - 1);
  },

  replaceWith: function (newLine) {
    this.array[this.currentLineNumber] = newLine;
  },

  entireString: function () {
    return this.array.join("\n");
  },

  getCurrentLineNumber: function () {
    if (this.hasNext()) {
      return this.currentLineNumber;
    }
  }
});

Transformer = function (input) {
  this.iterator = new ArrayIterator(input.split("\n"));
  this.inJSDoc = false;
};

extend(Transformer.prototype, {
  result: function () {
    return this.iterator.entireString();
  },

  convert: function () {
    while (this.searchNext()) {
      this.convertLine();
    }
    return this;
  },

  searchNext: function () {
    while (this.iterator.hasNext()) {
      if (this.isJSDoc(this.iterator.next())) {
        return true;
      }
    }
  },

  convertLine: function () {
    this.iterator.replaceWith(
      "applicationContext.comment(\"" +
      this.stripComment(this.iterator.current()) +
      "\");"
    );
  },

  getCurrentLineNumber: function () {
    return this.iterator.getCurrentLineNumber();
  },

  // helper

  stripComment: function (str) {
    return str.match(/^\s*\/?\*\*?\/?\s*(.*)$/)[1];
  },

  isJSDoc: function (str) {
    var matched;

    if (this.inJSDoc && str.match(/^\s*\*/)) {
      matched = true;
      if (str.match(/^\s*\*\//)) {
        this.inJSDoc = false;
      }
    } else if ((!this.inJSDoc) && str.match(/^\s*\/\*\*/)) {
      matched = true;
      this.inJSDoc = true;
    }

    return matched;
  }
});

transform = function (input) {
  var transformer = new Transformer(input);
  return transformer.convert().result();
};

// Only Exported for Tests, please use `transform`
exports.Transformer = Transformer;

// transform(str) returns the transformed String
exports.transform = transform;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
