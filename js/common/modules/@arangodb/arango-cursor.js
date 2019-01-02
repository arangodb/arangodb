/*jshint strict: false */
/* global more:true */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Arango Simple Query Language
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const arangodb = require('@arangodb');
const internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
// / @brief array query
// //////////////////////////////////////////////////////////////////////////////

function GeneralArrayCursor (documents, skip, limit, data) {
  this._documents = documents;
  this._countTotal = documents.length;
  this._countQuery = documents.length;
  this._skip = skip;
  this._limit = limit;
  this._cached = false;
  this._extra = { };

  var self = this;
  if (data !== null && data !== undefined && typeof data === 'object') {
    [ 'stats', 'warnings', 'profile', 'plan' ].forEach(function (d) {
      if (data.hasOwnProperty(d)) {
        self._extra[d] = data[d];
      }
    });
    this._cached = data.cached || false;
  }

  this.execute();
}

GeneralArrayCursor.prototype.isArangoResultSet = true;

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes an array query
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.execute = function () {
  if (this._skip === null) {
    this._skip = 0;
  }

  var len = this._documents.length;
  var s = 0;
  var e = len;

  // skip from the beginning
  if (0 < this._skip) {
    s = this._skip;

    if (e < s) {
      s = e;
    }
  }

  // skip from the end
  else if (this._skip < 0) {
    var skip = -this._skip;

    if (skip < e) {
      s = e - skip;
    }
  }

  // apply limit
  if (this._limit !== null) {
    if (s + this._limit < e) {
      e = s + this._limit;
    }
  }

  this._current = s;
  this._stop = e;

  this._countQuery = e - s;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print an all query
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype._PRINT = function (context) {
  let text = '[object GeneralArrayCursor';
  
  text += ', count: ' + this._documents.length;
  text += ', cached: ' + (this._cached ? 'true' : 'false');

  if (this.hasOwnProperty('_extra') &&
    this._extra.hasOwnProperty('warnings') && this._extra.warnings.length > 0) {
    text += ', warning(s): ';
    var last = null;
    for (var j = 0; j < this._extra.warnings.length; j++) {
      if (this._extra.warnings[j].code !== last) {
        if (last !== null) {
          text += ', ';
        }
        text += '"' + this._extra.warnings[j].code + ' ' + this._extra.warnings[j].message + '"';
        last = this._extra.warnings[j].code;
      }
    }
  }
  text += ']';
  
  let rows = [], i = 0;
//  this._pos = this._printPos || currentPos;
  while (++i <= 10 && this.hasNext()) {
    rows.push(this.next());
  }
 
  more = undefined;
  if (rows.length > 0) {
    var old = internal.startCaptureMode();
    internal.print(rows);
    text += '\n' + internal.stopCaptureMode(old);
  
    if (this.hasNext()) {
      text += "\ntype 'more' to show more documents\n";
      more = this; // assign cursor to global variable more!
    }
  }
    
  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns all elements of the cursor
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.toArray =
  GeneralArrayCursor.prototype.elements = function () {
    return this._documents;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the count of the cursor
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.count = function (applyPagination) {
  if (applyPagination === undefined || !applyPagination) {
    return this._countTotal;
  }

  return this._countQuery;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return an extra value of the cursor
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.getExtra = function () {
  return this._extra || { };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks if the cursor is exhausted
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.hasNext = function () {
  return this._current < this._stop;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the next result document
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.next = function () {
  if (this._current < this._stop) {
    return this._documents[this._current++];
  }

  return undefined;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns an iterator for the results
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype[Symbol.iterator] = function * () {
  while (this._current < this._stop) {
    yield this._documents[this._current++];
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief drops the result
// //////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.dispose = function () {
  this._documents = null;
  this._skip = null;
  this._limit = null;
  this._countTotal = null;
  this._countQuery = null;
  this._current = null;
  this._stop = null;
  this._extra = null;
};

exports.GeneralArrayCursor = GeneralArrayCursor;
