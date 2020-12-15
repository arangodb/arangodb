/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Arango Simple Query Language
// /
// / @file
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

var arangosh = require('@arangodb/arangosh');

var ArangoQueryCursor = require('@arangodb/arango-query-cursor').ArangoQueryCursor;

var sq = require('@arangodb/simple-query-common');

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryArray = sq.SimpleQueryArray;
var SimpleQueryByExample = sq.SimpleQueryByExample;
var SimpleQueryByCondition = sq.SimpleQueryByCondition;
var SimpleQueryFulltext = sq.SimpleQueryFulltext;
var SimpleQueryGeo = sq.SimpleQueryGeo;
var SimpleQueryNear = sq.SimpleQueryNear;
var SimpleQueryRange = sq.SimpleQueryRange;
var SimpleQueryWithin = sq.SimpleQueryWithin;
var SimpleQueryWithinRectangle = sq.SimpleQueryWithinRectangle;

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a query-by-condition
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      condition: this._condition
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

    var method = 'by-condition';
    if (this.hasOwnProperty('_type')) {
      data.index = this._index;

      switch (this._type) {
        case 'skiplist':
          method = 'by-condition-skiplist';
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a range query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      attribute: this._attribute,
      right: this._right,
      left: this._left,
      closed: this._type === 1
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

    var requestResult = this._collection._database._connection.PUT(
      '/_api/simple/range', data);

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty('count')) {
      this._countQuery = requestResult.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a near query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      latitude: this._latitude,
      longitude: this._longitude
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }

    if (this._distance !== null) {
      data.distance = this._distance;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      '/_api/simple/near', data);

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty('count')) {
      this._countQuery = requestResult.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a within query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      latitude: this._latitude,
      longitude: this._longitude,
      radius: this._radius
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }

    if (this._distance !== null) {
      data.distance = this._distance;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      '/_api/simple/within', data);

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty('count')) {
      this._countQuery = requestResult.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a withinRectangle query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      latitude1: this._latitude1,
      longitude1: this._longitude1,
      latitude2: this._latitude2,
      longitude2: this._longitude2
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }

    if (this._distance !== null) {
      data.distance = this._distance;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      '/_api/simple/within-rectangle', data);

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty('count')) {
      this._countQuery = requestResult.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a fulltext query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      attribute: this._attribute,
      query: this._query
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._index !== null) {
      data.index = this._index;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      '/_api/simple/fulltext', data);

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty('count')) {
      this._countQuery = requestResult.count;
    }
  }
};

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryByCondition = SimpleQueryByCondition;
exports.SimpleQueryFulltext = SimpleQueryFulltext;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryWithinRectangle = SimpleQueryWithinRectangle;
