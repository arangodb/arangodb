/* jshint strict: false */
/* global: arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

let arangosh = require('@arangodb/arangosh');

function ArangoReplicatedLog(database, id) {
  this._database = database;
  this._id = id;
  this._dbPrefix = '/_db/' + encodeURIComponent(database._name());
}

exports.ArangoReplicatedLog = ArangoReplicatedLog;

ArangoReplicatedLog.prototype.id = function () {
  return this._id;
};

ArangoReplicatedLog.prototype._prefixurl = function (url) {
  if (url.substr(0, 5) === '/_db/') {
    return url;
  }

  if (url[0] === '/') {
    return this._dbPrefix + url;
  }
  return this._dbPrefix + '/' + url;
};

ArangoReplicatedLog.prototype._baseurl = function (suffix) {
  let url = this._database._replicatedlogurl(this.id());

  if (suffix) {
    url += '/' + suffix;
  }

  return this._prefixurl(url);
};

ArangoReplicatedLog.prototype.drop = function() {
  let requestResult = this._database._connection.DELETE(this._baseurl());
  arangosh.checkRequestResult(requestResult);
};

ArangoReplicatedLog.prototype.status = function() {
  let requestResult = this._database._connection.GET(this._baseurl());
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.head = function(limit) {
  let query = '/head';
  if (limit !== undefined) {
    query += `?limit=${limit}`;
  }
  let requestResult = this._database._connection.GET(this._baseurl() + query);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.tail = function(limit) {
  let query = '/tail';
  if (limit !== undefined) {
    query += `?limit=${limit}`;
  }
  let requestResult = this._database._connection.GET(this._baseurl() + query);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.slice = function(start, stop) {
  let query = '/slice?';
  if (start !== undefined) {
    query += `start=${start}`;
  }
  if (stop !== undefined) {
    if (start !== undefined) {
      query += `&`;
    }
    query += `stop=${stop}`;
  }
  let requestResult = this._database._connection.GET(this._baseurl() + query);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.poll = function(first, limit) {
  let query = '/poll?';
  if (first !== undefined) {
    query += `first=${first}`;
  }
  if (limit !== undefined) {
    if (first !== undefined) {
      query += `&`;
    }
    query += `limit=${limit}`;
  }
  let requestResult = this._database._connection.GET(this._baseurl() + query);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.at = function(index) {
  let requestResult = this._database._connection.GET(this._baseurl() + `/entry/${index}`);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.release = function(index) {
  let requestResult = this._database._connection.POST(this._baseurl() + `/release?index=${index}`, {});
  arangosh.checkRequestResult(requestResult);
};

ArangoReplicatedLog.prototype.insert = function (payload, waitForSync = false) {
  let str = JSON.stringify(payload);
  let requestResult = this._database._connection.POST(this._baseurl() + `/insert?waitForSync=${waitForSync || false}`, str);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.multiInsert = function(payload, waitForSync = false) {
  let str = JSON.stringify(payload);
  let requestResult = this._database._connection.POST(this._baseurl() + `/multi-insert?waitForSync=${waitForSync || false}`, str);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoReplicatedLog.prototype.toString = function () {
  return `[object ArangoReplicatedLog ${this.id()}]`;
};
