'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB utilities
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2016 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Alan Plum
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');

exports.union = function union() {
  const things = Array.prototype.slice.call(arguments);
  if (things.length < 2) {
    return things[0];
  }
  let result;
  if (things[0] instanceof Map) {
    result = new Map();
    for (let map of things) {
      for (let entry of map.entries()) {
        result.set(entry[0], entry[1]);
      }
    }
  } else if (things[0] instanceof Set) {
    result = new Set();
    for (let set of things) {
      for (let value of set.values()) {
        result.add(value);
      }
    }
  } else if (Array.isArray(things[0])) {
    result = Array.prototype.concat.apply([], things);
  } else {
    things.unshift({});
    result = _.extend.apply(_, things);
  }
  return result;
};
