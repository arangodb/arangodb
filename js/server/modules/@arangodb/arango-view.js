/*jshint strict: false */
/*global require, exports, module */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoView
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2013 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

var ArangoView = internal.ArangoView;

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a view
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._PRINT = function (context) {
  var colors = require('internal').COLORS;
  var useColor = context.useColor;

  context.output += '[ArangoView ';
  if (useColor) { context.output += colors.COLOR_NUMBER; }
  context.output += this._id;
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += ', "';
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += this.name() || 'unknown';
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += '" (type ' + this.type() + ')]';
};

exports.ArangoView = ArangoView;
