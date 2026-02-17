/* jshint strict: false */
/* global SYS_IS_V8_BUILD */

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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangosh = require('@arangodb/arangosh');

var ArangoQueryCursor = require('@arangodb/arango-query-cursor').ArangoQueryCursor;

var sq = require('@arangodb/simple-query-common');

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryByExample = sq.SimpleQueryByExample;

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name()
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    // for _api/simple/all stream becomes a top-level option
    data.stream = true;

    var requestResult = this._collection._database._connection.PUT(
      '/_api/simple/all', data);

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, 
                                            requestResult, true);

    if (requestResult.hasOwnProperty('count')) {
      this._countQuery = requestResult.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a query-by-example
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      example: this._example
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var method = 'by-example';
    if (this.hasOwnProperty('_type')) {
      data.index = this._index;

      switch (this._type) {
        case 'hash':
          method = 'by-example-hash';
          break;
        case 'skiplist':
          method = 'by-example-skiplist';
          break;
      }
    }

    var requestResult = this._collection._database._connection.PUT(
      '/_api/simple/' + method, data);

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty('count')) {
      this._countQuery = requestResult.count;
      this._countTotal = requestResult.count;
    }
  }
};

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryByExample = SimpleQueryByExample;
