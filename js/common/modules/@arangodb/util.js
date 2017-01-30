'use strict';
// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB utilities
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2016 triagens GmbH, Cologne, Germany
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
// / @author Alan Plum
// / @author Copyright 2016, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');

exports.pluck = function (objs, key) {
  return _.map(objs, function (obj) {
    return _.get(obj, key);
  });
};

exports.union = function union () {
  const things = Array.prototype.slice.call(arguments);
  if (!things.slice(1).some(Boolean)) {
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
    result = Object.assign(...things);
  }
  return result;
};

exports.drain = function (generator) {
  let results = [];
  for (let result of generator) {
    results.push(result);
  }
  return results;
};

exports.inline = function (strs) {
  let str;
  if (typeof strs === 'string') {
    str = strs;
  } else {
    const strb = [strs[0]];
    const vars = Array.prototype.slice.call(arguments, 1);
    for (let i = 0; i < vars.length; i++) {
      strb.push(vars[i], strs[i + 1]);
    }
    str = strb.join('');
  }
  return str.replace(/\s*\n\s*/g, ' ').replace(/(^\s|\s$)/g, '');
};

exports.propertyKeys = function (obj) {
  return Object.keys(obj).filter((key) => (
    key.charAt(0) !== '_' && key.charAt(0) !== '$'
  ));
};

exports.shallowCopy = function (src) {
  const dest = {};
  if (src === undefined || src === null) {
    return dest;
  }
  for (const key of exports.propertyKeys(src)) {
    dest[key] = src[key];
  }
  return dest;
};
