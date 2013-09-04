/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true */
/*global module, require, exports */

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

var Preprocessor,
  preprocess,
  ArrayIterator,
  extend = require("underscore").extend;

ArrayIterator = function (arr) {
  'use strict';
  this.array = arr;
  this.currentLineNumber = -1;
};

extend(ArrayIterator.prototype, {
  next: function () {
    'use strict';
    this.currentLineNumber += 1;
    return this.array[this.currentLineNumber];
  },

  current: function () {
    'use strict';
    return this.array[this.currentLineNumber];
  },

  hasNext: function () {
    'use strict';
    return this.currentLineNumber < (this.array.length - 1);
  },

  replaceWith: function (newLine) {
    'use strict';
    this.array[this.currentLineNumber] = newLine;
  },

  entireString: function () {
    'use strict';
    return this.array.join("\n");
  },

  getCurrentLineNumber: function () {
    'use strict';
    if (this.hasNext()) {
      return this.currentLineNumber;
    }
  }
});

Preprocessor = function (input) {
  'use strict';
  this.iterator = new ArrayIterator(input.split("\n"));
  this.inJSDoc = false;
};

extend(Preprocessor.prototype, {
  result: function () {
    'use strict';
    return this.iterator.entireString();
  },

  convert: function () {
    'use strict';
    while (this.searchNext()) {
      this.convertLine();
    }
    return this;
  },

  searchNext: function () {
    'use strict';
    while (this.iterator.hasNext()) {
      if (this.isJSDoc(this.iterator.next())) {
        return true;
      }
    }
  },

  convertLine: function () {
    'use strict';
    this.iterator.replaceWith(
      "applicationContext.comment(\"" + this.stripComment(this.iterator.current()) + "\");"
    );
  },

  getCurrentLineNumber: function () {
    'use strict';
    return this.iterator.getCurrentLineNumber();
  },

  // helper

  stripComment: function (str) {
    'use strict';
    return str.match(/^\s*\/?\*\*?\/?\s*(.*)$/)[1];
  },

  isJSDoc: function (str) {
    'use strict';
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

preprocess = function (input) {
  'use strict';
  var processer = new Preprocessor(input);
  return processer.convert().result();
};

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
