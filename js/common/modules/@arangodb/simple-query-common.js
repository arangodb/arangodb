/*jshint strict: false */

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

const arangodb = require('@arangodb');
const GeneralArrayCursor = require("@arangodb/arango-cursor").GeneralArrayCursor;

var ArangoError = arangodb.ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief simple query
// //////////////////////////////////////////////////////////////////////////////

function SimpleQuery () {
  this._execution = null;
  this._skip = 0;
  this._limit = null;
  this._countQuery = null;
  this._countTotal = null;
  this._batchSize = null;
}

SimpleQuery.prototype.isArangoResultSet = true;


// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a query
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.clone = function () {
  throw 'cannot clone abstract query';
};

SimpleQuery.prototype.execute = function () {
  throw 'cannot execute abstract query';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief converts into an array
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.toArray = function () {
  var result;

  this.execute();

  result = [];

  while (this.hasNext()) {
    result.push(this.next());
  }

  return result;
};

SimpleQuery.prototype.getBatchSize = function () {
  return this._batchSize;
};

SimpleQuery.prototype.setBatchSize = function (value) {
  if (value >= 1) {
    this._batchSize = value;
  }
};

SimpleQuery.prototype.count = function (applyPagination) {
  this.execute();

  if (applyPagination === undefined || ! applyPagination) {
    return this._countTotal;
  }

  return this._countQuery;
};

SimpleQuery.prototype.hasNext = function () {
  this.execute();

  return this._execution.hasNext();
};

SimpleQuery.prototype.next = function () {
  this.execute();

  return this._execution.next();
};

SimpleQuery.prototype[Symbol.iterator] = function * () {
  this.execute();
  for (const item of this._execution) {
    yield item;
  }
};

SimpleQuery.prototype.dispose = function () {
  if (this._execution !== null) {
    this._execution.dispose();
  }

  this._execution = null;
  this._countQuery = null;
  this._countTotal = null;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief all query
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryAll (collection) {
  this._collection = collection;
}

SimpleQueryAll.prototype = new SimpleQuery();
SimpleQueryAll.prototype.constructor = SimpleQueryAll;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.clone = function () {
  var query;

  query = new SimpleQueryAll(this._collection);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryAll(' + this._collection.name() + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief query-by-example
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExample (collection, example) {
  this._collection = collection;
  this._example = example;
}

SimpleQueryByExample.prototype = new SimpleQuery();
SimpleQueryByExample.prototype.constructor = SimpleQueryByExample;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a query-by-example
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.clone = function () {
  var query;

  query = new SimpleQueryByExample(this._collection, this._example);
  query._skip = this._skip;
  query._limit = this._limit;
  query._type = this._type;
  query._index = this._index;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print a query-by-example
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryByExample(' + this._collection.name() + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryByExample = SimpleQueryByExample;
