/* jshint strict: false, unused: true, bitwise: false, esnext: true */
/* global AQL_WARNING  */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Aql, internal query functions
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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var INTERNAL = require('internal');
var ArangoError = require('@arangodb').ArangoError;
var isCoordinator = require('@arangodb/cluster').isCoordinator();

// //////////////////////////////////////////////////////////////////////////////
// / @brief user functions cache
// //////////////////////////////////////////////////////////////////////////////

var UserFunctions = { };

// //////////////////////////////////////////////////////////////////////////////
// / @brief prefab traversal visitors
// //////////////////////////////////////////////////////////////////////////////

var DefaultVisitors = {
  '_AQL::HASATTRIBUTESVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex, path) {
      if (typeof config.data === 'object' && Array.isArray(config.data.attributes)) {
        if (config.data.attributes.length === 0) {
          return;
        }

        var allowNull = true; // default value
        if (config.data.hasOwnProperty('allowNull')) {
          allowNull = config.data.allowNull;
        }
        var i;
        if (config.data.type === 'any') {
          for (i = 0; i < config.data.attributes.length; ++i) {
            if (!vertex.hasOwnProperty(config.data.attributes[i])) {
              continue;
            }
            if (!allowNull && vertex[config.data.attributes[i]] === null) {
              continue;
            }

            return CLONE({ vertex: vertex, path: path });
          }

          return;
        }

        for (i = 0; i < config.data.attributes.length; ++i) {
          if (!vertex.hasOwnProperty(config.data.attributes[i])) {
            return;
          }
          if (!allowNull &&
            vertex[config.data.attributes[i]] === null) {
            return;
          }
        }

        return CLONE({ vertex: vertex, path: path });
      }
    }
  },
  '_AQL::PROJECTINGVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex) {
      var values = { };
      if (typeof config.data === 'object' && Array.isArray(config.data.attributes)) {
        config.data.attributes.forEach(function (attribute) {
          values[attribute] = vertex[attribute];
        });
      }
      return values;
    }
  },
  '_AQL::IDVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex) {
      return vertex._id;
    }
  },
  '_AQL::KEYVISITOR': {
    visitorReturnsResults: true,
    func: function (config, result, vertex) {
      return vertex._key;
    }
  },
  '_AQL::COUNTINGVISITOR': {
    visitorReturnsResults: false,
    func: function (config, result) {
      if (result.length === 0) {
        result.push(0);
      }
      result[0]++;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief type weight used for sorting and comparing
// //////////////////////////////////////////////////////////////////////////////

var TYPEWEIGHT_NULL = 0;
var TYPEWEIGHT_BOOL = 1;
var TYPEWEIGHT_NUMBER = 2;
var TYPEWEIGHT_STRING = 4;
var TYPEWEIGHT_ARRAY = 8;
var TYPEWEIGHT_OBJECT = 16;

// //////////////////////////////////////////////////////////////////////////////
// / @brief raise a warning
// //////////////////////////////////////////////////////////////////////////////

function WARN (func, error, data) {
  'use strict';

  if (error.code === INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code) {
    AQL_WARNING(error.code, error.message.replace(/%s/, func));
  } else {
    var prefix = '';
    if (func !== null) {
      prefix = "in function '" + func + "()': ";
    }

    if (typeof data === 'string') {
      AQL_WARNING(error.code, prefix + error.message.replace(/%s/, data));
    } else {
      AQL_WARNING(error.code, prefix + error.message);
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief throw a runtime exception
// //////////////////////////////////////////////////////////////////////////////

function THROW (func, error, data, moreMessage) {
  'use strict';

  var prefix = '';
  if (func !== null && func !== '') {
    prefix = "in function '" + func + "()': ";
  }

  var err = new ArangoError();

  err.errorNum = error.code;
  if (typeof data === 'string') {
    err.errorMessage = prefix + error.message.replace(/%s/, data);
  } else {
    err.errorMessage = prefix + error.message;
  }
  if (moreMessage !== undefined) {
    err.errorMessage += '; ' + moreMessage;
  }

  throw err;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a database-specific function prefix
// //////////////////////////////////////////////////////////////////////////////

function DB_PREFIX () {
  return INTERNAL.db._name();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief reset the user functions and reload them from the database
// //////////////////////////////////////////////////////////////////////////////

function reloadUserFunctions () {
  'use strict';

  var prefix = DB_PREFIX();
  var c = INTERNAL.db._collection('_aqlfunctions');

  if (c === null) {
    // collection not found. now reset all user functions
    UserFunctions = { };
    UserFunctions[prefix] = { };
    return;
  }

  var foundError = false;
  var functions = { };

  c.toArray().forEach(function (f) {
    var key = f._key.replace(/:{1,}/g, '::');
    var code;

    if (f.code.match(/^\(?function\s+\(/)) {
      code = f.code;
    } else {
      code = '(function() { var callback = ' + f.code + ';\n return callback; })();';
    }

    try {
      var res = INTERNAL.executeScript(code, undefined, '(user function ' + key + ')');
      if (typeof res !== 'function') {
        foundError = true;
      }

      functions[key.toUpperCase()] = {
        name: key,
        func: res,
        isDeterministic: f.isDeterministic || false
      };
    } catch (err) {
      // in case a single function is broken, we still continue with the other ones
      // so that at least some functions remain usable
      foundError = true;
    }
  });

  // now reset the functions for all databases
  // this ensures that functions of other databases will be reloaded next
  // time (the reload does not necessarily need to be carried out in the
  // database in which the function is registered)
  UserFunctions = { };
  UserFunctions[prefix] = functions;

  if (foundError) {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_INVALID_CODE);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a user-function by name
// //////////////////////////////////////////////////////////////////////////////

function GET_USERFUNCTION (name, config) {
  var prefix = DB_PREFIX(), reloaded = false;
  var key = name.toUpperCase();

  var func;

  if (DefaultVisitors.hasOwnProperty(key)) {
    var visitor = DefaultVisitors[key];
    func = visitor.func;
    config.visitorReturnsResults = visitor.visitorReturnsResults;
  } else {
    if (!UserFunctions.hasOwnProperty(prefix)) {
      reloadUserFunctions();
      reloaded = true;
    }

    if (!UserFunctions[prefix].hasOwnProperty(key) && !reloaded) {
      // last chance
      reloadUserFunctions();
    }

    if (!UserFunctions[prefix].hasOwnProperty(key)) {
      THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
    }

    func = UserFunctions[prefix][key].func;
  }

  if (typeof func !== 'function') {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  return func;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief normalize a function name
// //////////////////////////////////////////////////////////////////////////////

function NORMALIZE_FNAME (functionName) {
  'use strict';

  var p = functionName.indexOf('::');

  if (p === -1) {
    return functionName;
  }

  return functionName.substr(p + 2);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get access to a collection
// //////////////////////////////////////////////////////////////////////////////

function COLLECTION (name, func) {
  'use strict';

  if (typeof name !== 'string') {
    THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func);
  }

  var c;
  if (name.substring(0, 1) === '_') {
    // system collections need to be accessed slightly differently as they
    // are not returned by the propertyGetter of db
    c = INTERNAL.db._collection(name);
  } else {
    c = INTERNAL.db[name];
  }

  if (c === null || c === undefined) {
    THROW(func, INTERNAL.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, String(name));
  }
  return c;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief clone an object
// //////////////////////////////////////////////////////////////////////////////

function CLONE (obj) {
  'use strict';

  if (obj === null || typeof (obj) !== 'object') {
    return obj;
  }

  var copy;
  if (Array.isArray(obj)) {
    copy = [];
    obj.forEach(function (i) {
      copy.push(CLONE(i));
    });
  } else if (obj instanceof Object) {
    copy = { };
    Object.keys(obj).forEach(function (k) {
      copy[k] = CLONE(obj[k]);
    });
  }

  return copy;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief box a value into the AQL datatype system
// //////////////////////////////////////////////////////////////////////////////

function FIX_VALUE (value) {
  'use strict';

  var type = typeof (value);

  if (value === undefined ||
    value === null ||
    (type === 'number' && (isNaN(value) || !isFinite(value)))) {
    return null;
  }

  if (type === 'boolean' || type === 'string' || type === 'number') {
    return value;
  }

  if (Array.isArray(value)) {
    var i, n = value.length;
    for (i = 0; i < n; ++i) {
      value[i] = FIX_VALUE(value[i]);
    }

    return value;
  }

  if (type === 'object') {
    var result = { };

    Object.keys(value).forEach(function (k) {
      if (typeof value[k] !== 'function') {
        result[k] = FIX_VALUE(value[k]);
      }
    });

    return result;
  }

  return null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the sort type of an operand
// //////////////////////////////////////////////////////////////////////////////

function TYPEWEIGHT (value) {
  'use strict';

  if (value !== undefined && value !== null) {
    if (Array.isArray(value)) {
      return TYPEWEIGHT_ARRAY;
    }

    switch (typeof (value)) {
      case 'boolean':
        return TYPEWEIGHT_BOOL;
      case 'number':
        if (isNaN(value) || !isFinite(value)) {
          // not a number => null
          return TYPEWEIGHT_NULL;
        }
        return TYPEWEIGHT_NUMBER;
      case 'string':
        return TYPEWEIGHT_STRING;
      case 'object':
        return TYPEWEIGHT_OBJECT;
    }
  }

  return TYPEWEIGHT_NULL;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief call a user function
// //////////////////////////////////////////////////////////////////////////////

function FCALL_USER (name, parameters, func) {
  'use strict';

  var prefix = DB_PREFIX(), reloaded = false;
  if (!UserFunctions.hasOwnProperty(prefix)) {
    reloadUserFunctions();
    reloaded = true;
  }

  if (!UserFunctions[prefix].hasOwnProperty(name) && !reloaded) {
    // last chance
    reloadUserFunctions();
  }

  if (!UserFunctions[prefix].hasOwnProperty(name)) {
    THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  try {
    return FIX_VALUE(UserFunctions[prefix][name].func.apply({ name: name }, parameters));
  } catch (err) {
    THROW(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, AQL_TO_STRING(err.stack || String(err)));
    return null;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the numeric value or undefined if it is out of range
// //////////////////////////////////////////////////////////////////////////////

function NUMERIC_VALUE (value, nullify) {
  'use strict';

  if (value === null || isNaN(value) || !isFinite(value)) {
    if (nullify) {
      return null;
    }
    return 0;
  }

  return value;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief cast to a number
// /
// / the operand can have any type, returns a number or null
// //////////////////////////////////////////////////////////////////////////////

function AQL_TO_NUMBER (value) {
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      // this covers Infinity and NaN
      return 0;
    case TYPEWEIGHT_BOOL:
      return (value ? 1 : 0);
    case TYPEWEIGHT_NUMBER:
      return value;
    case TYPEWEIGHT_STRING:
      var result = NUMERIC_VALUE(Number(value), false);
      return ((TYPEWEIGHT(result) === TYPEWEIGHT_NUMBER) ? result : null);
    case TYPEWEIGHT_ARRAY:
      if (value.length === 0) {
        return 0;
      }
      if (value.length === 1) {
        return AQL_TO_NUMBER(value[0]);
      }
  // fallthrough intentional
  }
  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief cast to a string
// /
// / the operand can have any type, always returns a string
// //////////////////////////////////////////////////////////////////////////////

function AQL_TO_STRING (value) {
  'use strict';

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return '';
    case TYPEWEIGHT_BOOL:
      return (value ? 'true' : 'false');
    case TYPEWEIGHT_STRING:
      return value;
    case TYPEWEIGHT_NUMBER:
      return value.toString();
    case TYPEWEIGHT_ARRAY:
    case TYPEWEIGHT_OBJECT:
      return JSON.stringify(value);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return documents within a bounding rectangle
// //////////////////////////////////////////////////////////////////////////////

function AQL_WITHIN_RECTANGLE (collection, latitude1, longitude1, latitude2, longitude2) {
  'use strict';

  if (TYPEWEIGHT(latitude1) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(longitude1) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(latitude2) !== TYPEWEIGHT_NUMBER ||
    TYPEWEIGHT(longitude2) !== TYPEWEIGHT_NUMBER) {
    WARN('WITHIN_RECTANGLE', INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return null;
  }

  return COLLECTION(collection, 'WITHIN_RECTANGLE').withinRectangle(
    latitude1,
    longitude1,
    latitude2,
    longitude2
  ).toArray();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief passthru the argument
// /
// / this function is marked as non-deterministic so its argument withstands
// / query optimisation. this function can be used for testing
// //////////////////////////////////////////////////////////////////////////////

function AQL_PASSTHRU (value) {
  'use strict';

  return value;
}

function AQL_PREGEL_RESULT (executionNr) {
  'use strict';

  if (isCoordinator) {
      return INTERNAL.db._pregelAqlResult(executionNr);
  } else {
    THROW('PREGEL_RESULT', INTERNAL.errors.ERROR_CLUSTER_ONLY_ON_COORDINATOR);
  }
}

exports.FCALL_USER = FCALL_USER;
exports.AQL_WITHIN_RECTANGLE = AQL_WITHIN_RECTANGLE;
exports.AQL_V8 = AQL_PASSTHRU;
exports.AQL_PREGEL_RESULT = AQL_PREGEL_RESULT;

exports.reload = reloadUserFunctions;
exports.fixValue = FIX_VALUE;
