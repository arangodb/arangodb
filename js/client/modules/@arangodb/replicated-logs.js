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

function ReplicatedLog(database, id) {
  this._database = database;
  this._id = id;
  this._dbPrefix = '/_db/' + encodeURIComponent(database._name());
}

exports.ReplicatedLog = ReplicatedLog;

ReplicatedLog.prototype.id = function () {
  return this._id;
};

ReplicatedLog.prototype._prefixurl = function (url) {
  if (url.substr(0, 5) === '/_db/') {
    return url;
  }

  if (url[0] === '/') {
    return this._dbPrefix + url;
  }
  return this._dbPrefix + '/' + url;
};

ReplicatedLog.prototype._baseurl = function (suffix) {
  let url = this._database._replicatedlogurl(this.id());

  if (suffix) {
    url += '/' + suffix;
  }

  return this._prefixurl(url);
};

ReplicatedLog.prototype.drop = function() {
  let requestResult = this._database._connection.DELETE(this._baseurl());
  arangosh.checkRequestResult(requestResult);
};

ReplicatedLog.prototype.status = function() {
  let requestResult = this._database._connection.GET(this._baseurl());
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ReplicatedLog.prototype.head = function(limit = 100) {
  let requestResult = this._database._connection.GET(this._baseurl() + `/head?limit=${limit}`);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ReplicatedLog.prototype.tail = function(limit = 100) {
  let requestResult = this._database._connection.GET(this._baseurl() + `/tail?limit=${limit}`);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ReplicatedLog.prototype.slice = function(start, stop) {
  let requestResult = this._database._connection.GET(this._baseurl() + `/slice?start=${start}&stop=${stop}`);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ReplicatedLog.prototype.poll = function(first, limit = 100) {
  let requestResult = this._database._connection.GET(this._baseurl() + `/poll?first=${first}&limit=${limit}`);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ReplicatedLog.prototype.at = function(index) {
  let requestResult = this._database._connection.GET(this._baseurl() + `/entry/${index}`);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ReplicatedLog.prototype.release = function(index) {
  let requestResult = this._database._connection.POST(this._baseurl() + `/release?index=${index}`, {});
  arangosh.checkRequestResult(requestResult);
};

ReplicatedLog.prototype.insert = function (payload, waitForSync = false) {
  let str = JSON.stringify(payload);
  let requestResult = this._database._connection.POST(this._baseurl() + `/insert?waitForSync=${waitForSync || false}`, str);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ReplicatedLog.prototype.toString = function () {
  return `[object ReplicatedLog ${this.id()}]`;
};
