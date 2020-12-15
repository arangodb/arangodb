'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Check if something is something
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler, Lucas Dohmen
// / @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// Check if a value is not undefined or null
function existy (x) {
  return x !== null && x !== undefined;
}

// Check if a value is undefined or null
function notExisty (x) {
  return !existy(x);
}

// Check if a value is existy and not false
function truthy (x) {
  return (x !== false) && existy(x);
}

// Check if a value is not truthy
function falsy (x) {
  return !truthy(x);
}

// is.object, is.noObject, is.array, is.noArray...
[
  'Object',
  'Array',
  'Boolean',
  'Date',
  'Function',
  'Number',
  'String',
  'RegExp'
].forEach(function (type) {
  exports[type.toLowerCase()] = function (obj) {
    return Object.prototype.toString.call(obj) === '[object ' + type + ']';
  };

  exports['no' + type] = function (obj) {
    return Object.prototype.toString.call(obj) !== '[object ' + type + ']';
  };
});

exports.existy = existy;
exports.notExisty = notExisty;
exports.truthy = truthy;
exports.falsy = falsy;
