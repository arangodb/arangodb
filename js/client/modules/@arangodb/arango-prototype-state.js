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

function ArangoPrototypeState(database, id) {
  this._database = database;
  this._id = id;
  this._dbPrefix = '/_db/' + encodeURIComponent(database._name());
}

exports.ArangoPrototypeState = ArangoPrototypeState;

ArangoPrototypeState.prototype.id = function () {
  return this._id;
};

ArangoPrototypeState.prototype._prefixurl = function (url) {
  if (url.substr(0, 5) === '/_db/') {
    return url;
  }

  if (url[0] === '/') {
    return this._dbPrefix + url;
  }
  return this._dbPrefix + '/' + url;
};

ArangoPrototypeState.prototype._baseurl = function (suffix) {
  let url = this._database._prototypestateurl(this.id());

  if (suffix) {
    url += '/' + suffix;
  }

  return this._prefixurl(url);
};

ArangoPrototypeState.prototype.drop = function () {
  let requestResult = this._database._connection.DELETE(this._baseurl());
  arangosh.checkRequestResult(requestResult);
};

ArangoPrototypeState.prototype._writeInternal = function (values, options) {
  let query = `/insert?waitForCommit=${options.waitForCommit || true}`
      + `&waitForSync=${options.waitForSync || false}`
      + `&waitForApplied=${options.waitForApplied || true}`;
  let requestResult = this._database._connection.POST(this._baseurl() + query, values);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result.index;
};


ArangoPrototypeState.prototype._readInternal = function (keys, options) {
  let query = `/multi-get?waitForApplied=${options.waitForApplied || 0}`;
  if (options.readFrom !== undefined) {
    query += `&readFrom=${encodeURIComponent(options.readFrom)}`;
  }
  let requestResult = this._database._connection.POST(this._baseurl() + query, keys);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoPrototypeState.prototype.getSnapshot = function (waitForIndex) {
  let query = `/snapshot`;
  if (waitForIndex !== undefined) {
    query += `?waitForIndex=${waitForIndex}`;
  }
  let requestResult = this._database._connection.GET(this._baseurl() + query);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

ArangoPrototypeState.prototype.toString = function () {
  return `[object ArangoPrototypeState ${this._database._name()}/${this.id()}]`;
};

require('@arangodb/arango-prototype-state-common');
