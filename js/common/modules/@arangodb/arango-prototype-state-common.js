/*jshint strict: false */
/*global ArangoClusterInfo, require, exports, module */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoCollection
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2013 triagens GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var ArangoPrototypeState = require('@arangodb/arango-prototype-state').ArangoPrototypeState;

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

exports.ArangoPrototypeState = ArangoPrototypeState;

// s.write("key", "value", {options})
// s.write({"key":"value", ...}, {options})
ArangoPrototypeState.prototype.write = function (...args) {
  if (args.length === 3) {
    return this.write({[args[0]]: args[1]}, args[2]);
  }
  if (args.length === 1) {
    return this.write(args[0], {});
  }
  if (args.length !== 2) {
    throw new Error("invalid arguments");
  }
  if (typeof args[0] === 'string' || args[0] instanceof String) {
    return this.write({[args[0]]: args[1]}, {});
  }

  const [values, options] = args;
  return this._writeInternal(values, options);
};

// s.read("key", {options})
// s.read(["key1", "key2"], {options})
ArangoPrototypeState.prototype.read = function (keys, options) {
  if (!(keys instanceof Array)) {
    const result = this.read([keys], options);
    return result[keys];
  }
  options = options || {};

  return this._readInternal(keys, options);
};
