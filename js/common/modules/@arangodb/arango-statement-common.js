/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Arango statements
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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function ArangoStatement (database, data) {
  this._database = database;
  this._doCount = false;
  this._batchSize = null;
  this._bindVars = {};
  this._options = undefined;
  this._cache = undefined;
  this._stream = false;

  if (!data) {
    throw 'ArangoStatement needs initial data';
  }

  if (typeof data === 'string') {
    data = { query: data };
  } else if (typeof data === 'object' && typeof data.toAQL === 'function') {
    data = { query: data.toAQL() };
  }

  if (!(data instanceof Object)) {
    throw 'ArangoStatement needs initial data';
  }

  if (data.query === undefined || data.query === '') {
    throw 'ArangoStatement needs a valid query attribute';
  }
  this.setQuery(data.query);

  if (data.bindVars instanceof Object) {
    this.bind(data.bindVars);
  }
  if (data.options instanceof Object) {
    this.setOptions(data.options);
  }
  if (data.count !== undefined) {
    this.setCount(data.count);
  }
  if (data.batchSize !== undefined) {
    this.setBatchSize(data.batchSize);
  }
  if (data.cache !== undefined) {
    this.setCache(data.cache);
  }
  if (data.stream !== undefined) {
    this._stream = data.stream;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief binds a parameter to the statement
// /
// / This function can be called multiple times, once for each bind parameter.
// / All bind parameters will be transferred to the server in one go when
// / execute() is called.
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.bind = function (key, value) {
  if (key instanceof Object) {
    if (value !== undefined) {
      throw 'invalid bind parameter declaration';
    }

    this._bindVars = key;
  } else if (typeof (key) === 'string') {
    this._bindVars[key] = value;
  } else if (typeof (key) === 'number') {
    var strKey = String(parseInt(key, 10));

    if (strKey !== String(key)) {
      throw 'invalid bind parameter declaration';
    }

    this._bindVars[strKey] = value;
  } else {
    throw 'invalid bind parameter declaration';
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the bind variables already set
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getBindVariables = function () {
  return this._bindVars;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets the cache flag for the statement
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getCache = function () {
  return this._cache;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets the count flag for the statement
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getCount = function () {
  return this._doCount;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets the maximum number of results documents the cursor will return
// / in a single server roundtrip.
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getBatchSize = function () {
  return this._batchSize;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets the user options
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getOptions = function () {
  return this._options;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets query string
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getQuery = function () {
  return this._query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the cache flag for the statement
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setCache = function (bool) {
  this._cache = bool ? true : false;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the count flag for the statement
// /
// / Setting the count flag will make the statement's result cursor return the
// / total number of result documents. The count flag is not set by default.
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setCount = function (bool) {
  this._doCount = bool ? true : false;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the maximum number of results documents the cursor will return
// / in a single server roundtrip.
// / The higher this number is, the less server roundtrips will be made when
// / iterating over the result documents of a cursor.
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setBatchSize = function (value) {
  var batch = parseInt(value, 10);

  if (batch > 0) {
    this._batchSize = batch;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the user options
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setOptions = function (value) {
  this._options = value;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the query string
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setQuery = function (query) {
  this._query = (query && typeof query.toAQL === 'function') ? query.toAQL() : query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief parses a query and return the results
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.parse = function () {
  throw 'cannot call abstract method parse()';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief explains a query and return the results
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.explain = function () {
  throw 'cannot call abstract method explain()';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the query
// /
// / Invoking execute() will transfer the query and all bind parameters to the
// / server. It will return a cursor with the query results in case of success.
// / In case of an error, the error will be printed
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.execute = function () {
  throw 'cannot call abstract method execute()';
};

exports.ArangoStatement = ArangoStatement;
