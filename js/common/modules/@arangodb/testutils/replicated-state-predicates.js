/*jshint strict: true */
'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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

const SH = require("@arangodb/testutils/replicated-state-helper");
const _ = require("lodash");

const isError = function (value) {
  return value instanceof Error;
};

const localKeyStatus = function (endpoint, db, col, key, available, value) {
  return function() {
    const data = SH.getLocalValue(endpoint, db, col, key);
    if (available === false && data.code === 404) {
      return true;
    }
    if (available === true && data._key === key) {
      if (value === undefined) {
        // We are not interested in any value, just making sure the key exists.
        return true;
      }
      if (data.value === value) {
        return true;
      }
    }
    return Error(`Wrong value returned by ${endpoint}/${db}/${col}/${key}, got: ${JSON.stringify(data)}, ` +
      `expected: ${JSON.stringify(value)}`);
  };
};

exports.localKeyStatus = localKeyStatus;
