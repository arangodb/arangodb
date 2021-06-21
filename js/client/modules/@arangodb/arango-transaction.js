/* jshint strict: true */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction shell support for transactions
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const ArangoError = require('@arangodb').ArangoError;
const ArangoQueryCursor = require('@arangodb/arango-query-cursor').ArangoQueryCursor;
    
function throwNotRunning() {
  throw new ArangoError({
    error: true,
    code: internal.errors.ERROR_TRANSACTION_INTERNAL.code,
    errorNum: internal.errors.ERROR_TRANSACTION_INTERNAL.code,
    errorMessage: internal.errors.ERROR_TRANSACTION_INTERNAL.message
  });
};

function ArangoTransaction (database, data) {
  this._id = 0;
  this._done = false;
  this._database = database;
  this._dbName = database._name();
  this._dbPrefix = '/_db/' + encodeURIComponent(database._name());

  if (data && typeof data === 'object' && data._id) {
    this._id = data._id;
    return;
  } else if (typeof data === 'string') {
    this._id = data;
    return;
  } else if (typeof data === 'number') {
    this._id = String(data);
    return;
  }

  if (!data || typeof (data) !== 'object') {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: 'usage: ArangoTransaction(<object>)'
    });
  }

  data = Object.assign({}, data);

  if (!data.collections || typeof data.collections !== 'object') {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: 'missing/invalid collections definition for transaction'
    });
  }

  if (data.collections.read) {
    if (!Array.isArray(data.collections.read)) {
      data.collections.read = [data.collections.read];
    }
    data.collections.read = data.collections.read.map(
      col => col.isArangoCollection ? col.name() : col
    );
  }
  if (data.collections.write) {
    if (!Array.isArray(data.collections.write)) {
      data.collections.write = [data.collections.write];
    }
    data.collections.write = data.collections.write.map(
      col => col.isArangoCollection ? col.name() : col
    );
  }
  if (data.collections.exclusive) {
    if (!Array.isArray(data.collections.exclusive)) {
      data.collections.exclusive = [data.collections.exclusive];
    }
    data.collections.exclusive = data.collections.exclusive.map(
      col => col.isArangoCollection ? col.name() : col
    );
  }

  if (data.action) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: 'action definition is not supported'
    });
  }

  this._collections = data.collections;
  let body = {collections: this._collections};
  // copy transaction options
  [ 'lockTimeout', 'maxTransactionSize', 'intermediateCommitSize', 'intermediateCommitCount', 'waitForSync' ].forEach(function(o) {
    if (data.hasOwnProperty(o)) {
      body[o] = data[o];
    }
  });

  let url = this._url() + '/begin';
  let requestResult = this._database._connection.POST(url, body);

  arangosh.checkRequestResult(requestResult);
  this._id = requestResult.result.id;
}

exports.ArangoTransaction = ArangoTransaction;

function ArangoTransactionCollection(trx, coll) {
  if (!trx || !coll || !coll.isArangoCollection) {
    throw "invaliid input";
  }
  this._transaction = trx;
  this._collection = coll;
}

ArangoTransaction.prototype._url = function () {
  return this._dbPrefix + '/_api/transaction';
};

ArangoTransaction.prototype.id = function() {
  return this._id;
};

ArangoTransaction.prototype._PRINT = function (context) {
  let colors = require('internal').COLORS;
  let useColor = context.useColor;

  context.output += '[ArangoTransaction ';
  if (useColor) { context.output += colors.COLOR_NUMBER; }
  context.output += this._id;
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += ']';
};

ArangoTransaction.prototype.collection = function(col) {
  if (col.isArangoCollection) {
    return new ArangoTransactionCollection(this, col);
  }
  return new ArangoTransactionCollection(this, this._database._collection(col));
};

ArangoTransaction.prototype.commit = function() {
  let url = this._url() + '/' + this._id;
  var requestResult = this._database._connection.PUT(url, "");
  arangosh.checkRequestResult(requestResult);
  this._done = true;
  return requestResult.result;
};

ArangoTransaction.prototype.abort = function() {
  let url = this._url() + '/' + this._id;
  var requestResult = this._database._connection.DELETE(url, "");
  arangosh.checkRequestResult(requestResult);
  this._done = true;
  return requestResult.result;
};

ArangoTransaction.prototype.status = function() {
  let url = this._url() + '/' + this._id;
  var requestResult = this._database._connection.GET(url);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoTransaction.prototype.running = function() {
  return this._id !== 0 && !this._done;
};

ArangoTransaction.prototype.query = function(query, bindVars, cursorOptions, options) {
  if (!this.running()) {
    throwNotRunning();
  }
  if (typeof query !== 'string' || query === undefined || query === '') {
    throw 'need a valid query string';
  }
  if (options === undefined && cursorOptions !== undefined) {
    options = cursorOptions;
  }

  let body = {
    query: query,
    count: (cursorOptions && cursorOptions.count) || false,
    bindVars: bindVars || undefined,
  };

  if (cursorOptions && cursorOptions.batchSize) {
    body.batchSize = cursorOptions.batchSize;
  }
  
  if (options) {
    body.options = options;
  }

  if (cursorOptions && cursorOptions.cache) {
    body.cache = cursorOptions.cache;
  }

  const headers = {'x-arango-trx-id' : this.id()};
  var requestResult = this._database._connection.POST('/_api/cursor', body, headers);
  arangosh.checkRequestResult(requestResult);

  let isStream = false;
  if (options && options.stream) {
    isStream = options.stream;
  }

  return new ArangoQueryCursor(this._database, requestResult, isStream);
};

ArangoTransactionCollection.prototype.name = function() {
  return this._collection.name();
};

ArangoTransactionCollection.prototype.document = function(id) {
  if (!this._transaction.running()) {
    throwNotRunning();
  }
  let opts = { transactionId : this._transaction.id() };
  return this._collection.document(id, opts);
};

ArangoTransactionCollection.prototype.save = 
ArangoTransactionCollection.prototype.insert = function(data, opts) {
  if (!this._transaction.running()) {
    throwNotRunning();
  }
  opts = opts || {};
  opts.transactionId = this._transaction.id();
  return this._collection.insert(data, opts);
};

ArangoTransactionCollection.prototype.remove = function(id, options) {
  if (!this._transaction.running()) {
    throwNotRunning();
  }
  if (!options) {
    options = {};
  }
  options.transactionId = this._transaction.id();
  return this._collection.remove(id, options);
};

ArangoTransactionCollection.prototype.replace = function(id, data, options) {
  if (!this._transaction.running()) {
    throwNotRunning();
  }
  if (!options) {
    options = {};
  }
  options.transactionId = this._transaction.id();
  return this._collection.replace(id, data, options);
};

ArangoTransactionCollection.prototype.update = function(id, data, options) {
  if (!this._transaction.running()) {
    throwNotRunning();
  }
  if (!options) {
    options = {};
  }
  options.transactionId = this._transaction.id();
  return this._collection.update(id, data, options);
};

ArangoTransactionCollection.prototype.truncate = function(opts) {
  if (!this._transaction.running()) {
    throwNotRunning();
  }
  opts = opts || {};
  opts.transactionId = this._transaction.id();
  return this._collection.truncate(opts);
};

ArangoTransactionCollection.prototype.count = function() {
  if (!this._transaction.running()) {
    throwNotRunning();
  }
  
  const url = this._collection._baseurl('count');
  const headers = {'x-arango-trx-id' : this._transaction.id()};
  let requestResult = this._transaction._database._connection.GET(url, headers);
  arangosh.checkRequestResult(requestResult);

  return requestResult.count;
};
