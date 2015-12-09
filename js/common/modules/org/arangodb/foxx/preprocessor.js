'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Preprocessor
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

var extend = require('underscore').extend;

function ArrayIterator(arr) {
  this.array = arr;
  this.currentLineNumber = -1;
}

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
      return this.array.join('\n');
  },

  getCurrentLineNumber: function () {
      if (this.hasNext()) {
      return this.currentLineNumber;
    }
  }
});

function Preprocessor(input) {
  this.iterator = new ArrayIterator(input.replace('\r', '').split('\n'));
  this.inJSDoc = false;
}

extend(Preprocessor.prototype, {
  result: function () {
    return this.iterator.entireString();
  },

  convert: function (filename) {
    if (filename && filename.indexOf('node_modules') !== -1) {
      return this;
    }
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
      `applicationContext.comment("${this.stripComment(this.iterator.current())}");`
    );
  },

  getCurrentLineNumber: function () {
      return this.iterator.getCurrentLineNumber();
  },

  // helper

  stripComment: function (str) {
      return str.replace(/^\s*\/\*\*/, '').       // start of JSDoc comment
               replace(/^(.*?)\*\/.*$/, '$1').  // end of JSDoc comment
               replace(/^\s*\*/, '').           // continuation of JSDoc comment
               replace(/\\/g, '\\\\').          // replace backslashes
               replace(/"/g, '\\"').            // replace quotes
               trim();                          // remove leading and trailing spaces
  },

  isJSDoc: function (str) {
      var matched;

    if (this.inJSDoc) {
      if (str.match(/\*\//)) {
        // end of multi-line JSDoc comment
        this.inJSDoc = false;
      }
      matched = true;
    } else {
      if (str.match(/^\s*\/\*\*(.*?\*)?\//)) {
        // a single-line JSDoc comment
        matched = true;
      }
      else if (str.match(/^\s*\/\*\*/)) {
        // beginning of a multi-line JSDoc comment
        matched = true;
        this.inJSDoc = true;
      }
    }

    return matched;
  }
});

function preprocess(input, filename) {
  var processor = new Preprocessor(input);
  return processor.convert(filename).result();
}

// Only Exported for Tests, please use `process`
exports.Preprocessor = Preprocessor;

// process(str) returns the processed String
exports.preprocess = preprocess;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
