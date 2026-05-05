/*jshint strict: false, unused: false, maxlen: 200 */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var ArangoCollection = require('@arangodb/arango-collection').ArangoCollection;

var arangodb = require('@arangodb');

var ArangoError = arangodb.ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief document collection
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.TYPE_DOCUMENT = 2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief edge collection
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.TYPE_EDGE = 3;

ArangoCollection.prototype.isArangoCollection = true;

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a collection
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._PRINT = function (context) {
  var type = 'unknown';
  var name = this.name();

  switch (this.type()) {
    case ArangoCollection.TYPE_DOCUMENT:
      type = 'document';
      break;
    case ArangoCollection.TYPE_EDGE:
      type = 'edge';
      break;
  }

  var colors = require('internal').COLORS;
  var useColor = context.useColor;

  context.output += '[ArangoCollection ';
  if (useColor) { context.output += colors.COLOR_NUMBER; }
  context.output += this._id;
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += ', "';
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += name || 'unknown';
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += '" (type ' + type + ')]';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief converts into a string
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toString = function () {
  return '[ArangoCollection: ' + this._id + ']';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief qualifies a given document key
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.documentId = function (documentKey) {
  if (documentKey && typeof documentKey !== "string") {
    documentKey = String(documentKey);
  }
  if (!arangodb.isValidDocumentKey(documentKey)) {
    throw new ArangoError({
      errorNum: arangodb.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code,
      errorMessage: arangodb.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.message
    });
  }
  return `${this.name()}/${documentKey}`;
};

ArangoCollection.prototype.removeByExample = function (example, waitForSync, limit) {
  throw 'cannot call abstract removeByExample function';
};

ArangoCollection.prototype.replaceByExample = function (example, newValue, waitForSync, limit) {
  throw 'cannot call abstract replaceByExample function';
};

ArangoCollection.prototype.updateByExample = function (example, newValue, keepNull, waitForSync, limit) {
  throw 'cannot call abstract updateExample function';
};
