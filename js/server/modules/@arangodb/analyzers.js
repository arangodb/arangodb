////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

const internal = require('internal');

////////////////////////////////////////////////////////////////////////////////
/// @brief exported members and functions
////////////////////////////////////////////////////////////////////////////////

var ArangoAnalyzers = internal.ArangoAnalyzers;
exports.analyzer = ArangoAnalyzers.analyzer;
exports.remove = ArangoAnalyzers.remove;
exports.save = ArangoAnalyzers.save;
exports.toArray = ArangoAnalyzers.toArray;

////////////////////////////////////////////////////////////////////////////////
/// @brief pretty print
////////////////////////////////////////////////////////////////////////////////

var ArangoAnalyzer = global.ArangoAnalyzer;
delete global.ArangoAnalyzer; // same as js/server/bootstrap/modules/internal.js
ArangoAnalyzer.prototype._PRINT = function(context) {
  var colors = require('internal').COLORS;
  var useColor = context.useColor;

  context.output += '[ArangoAnalyzer ';
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += this.name();
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += '" (type ' + this.type() + ')]';
};
