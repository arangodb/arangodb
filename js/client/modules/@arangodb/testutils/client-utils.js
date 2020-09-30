/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require*/

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for user access rights
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const helper = require('@arangodb/testutils/user-helper');
const pu = require('@arangodb/process-utils');
const download = require('internal').download;
const dbName = helper.dbName;

const arango = require('internal').arango;

const executeJS = (code, dbName) => {
  let httpOptions = pu.makeAuthorizationHeaders({
    username: 'root',
    password: ''
  });
  httpOptions.method = 'POST';
  httpOptions.timeout = 180;
  httpOptions.returnBodyOnError = true;
  return download(arango.getEndpoint().replace('tcp', 'http') + (dbName === 'system' ? '' : `/_db/${dbName}`) + '/_admin/execute?returnAsJSON=true',
    code,
    httpOptions);
};

const dropKeySpace = (keySpaceId) => {
  executeJS(`global.GLOBAL_CACHE_REMOVE_PREFIX('${keySpaceId}');`, dbName);
};

const dropKeySpaceSystem = (keySpaceId) => {
  executeJS(`global.GLOBAL_CACHE_REMOVE_PREFIX('${keySpaceId}');`, '_system');
};

const getKey = (keySpaceId, key) => {
  return executeJS(`return global.GLOBAL_CACHE_GET('${keySpaceId}-${key}')[0];`, dbName).body === 'true';
};

const getKeySystem = (keySpaceId, key) => {
  return executeJS(`return global.GLOBAL_CACHE_GET('${keySpaceId}-${key}')[0];`, '_system').body === 'true';
};

const setKey = (keySpaceId, name) => {
  return executeJS(`global.GLOBAL_CACHE_SET('${keySpaceId}-${name}', false);`, dbName);
};

const setKeySystem = (keySpaceId, name) => {
  return executeJS(`global.GLOBAL_CACHE_SET('${keySpaceId}-${name}', false);`, '_system');
};

const wait = (keySpaceId, key) => {
  for (let i = 0; i < 200; i++) {
    if (getKey(keySpaceId, key)) {
      break;
    }
    require('internal').wait(0.1);
  }
};

const waitSystem = (keySpaceId, key) => {
  for (let i = 0; i < 200; i++) {
    if (getKeySystem(keySpaceId, key)) {
      break;
    }
    require('internal').wait(0.1);
  }
};

exports.dropKeySpace = dropKeySpace;
exports.dropKeySpaceSystem = dropKeySpaceSystem;
exports.getKey = getKey;
exports.getKeySystem = getKeySystem;
exports.setKey = setKey;
exports.setKeySystem = setKeySystem;
exports.wait = wait;
exports.waitSystem = waitSystem;
