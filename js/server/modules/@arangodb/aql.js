/* jshint strict: false, unused: true, bitwise: false, esnext: true */

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

// / @brief return the cache key prefix 
const CACHE_PREFIX = 'aqlfunctions';

// / @brief context-local AQL user functions cache
let UserFunctions = { };

// / @brief throw a runtime exception
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

// / @brief reset the user functions and reload them from the database
function reloadUserFunctions () {
  'use strict';

  let [functions, cachedVersion] = global.GLOBAL_CACHE_GET(CACHE_PREFIX);
  if (!functions || !Array.isArray(functions)) {
    let c = INTERNAL.db._collection('_aqlfunctions');

    if (c) {
      // store result under new version
      cachedVersion = global.GLOBAL_CACHE_NEW_VERSION();
      functions = c.toArray().map(function(doc) {
        delete doc._rev;
        delete doc._id;
        return doc;
      });
  
      global.GLOBAL_CACHE_SET(CACHE_PREFIX, functions, cachedVersion);
    }
  }

  let foundError = false;
  let compiled = { };
    
  functions.forEach(function (f) {
    let key = (f._key || f.name).replace(/:{1,}/g, '::');
    let code;

    if (f.code.match(/^\(?function\s+\(/)) {
      code = f.code;
    } else {
      code = '(function() { var callback = ' + f.code + ';\n return callback; })();';
    }

    try {
      let res = INTERNAL.executeScript(code, undefined, '(user function ' + key + ')');
      if (typeof res !== 'function') {
        foundError = true;
      }

      compiled[key.toUpperCase()] = {
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

  UserFunctions[INTERNAL.db._name()] = { funcs: compiled, version: cachedVersion };

  if (foundError) {
    THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_INVALID_CODE);
  }
}

// / @brief box a value into the AQL datatype system
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

// / @brief call a user-defined function
exports.FCALL_USER = function (name, parameters, func) {
  'use strict';
  
  const dbName = INTERNAL.db._name();

  let functionObject;
  if (UserFunctions.hasOwnProperty(dbName)) {
    let precompiled = UserFunctions[dbName];
    functionObject = precompiled.funcs[name];
    if (functionObject) {
      let cachedVersion  = global.GLOBAL_CACHE_GET_VERSION(CACHE_PREFIX);
      if (cachedVersion !== precompiled.version) {
        // what we have in our local UserFunctions cache is outdated
        functionObject = undefined;
      }
    }
  }

  if (!functionObject) {
    // rebuild UserFunctions object for current database
    reloadUserFunctions();
    functionObject = UserFunctions[dbName].funcs[name];
    if (!functionObject) {
      THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
    }
  }

  try {
    return FIX_VALUE(functionObject.func.apply({ name }, parameters));
  } catch (err) {
    THROW(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, String(err.stack || String(err)));
    return null;
  }
};

// / @brief passthru the argument
// / this function is marked as non-deterministic so its argument withstands
// / query optimization. this function can be used for testing
exports.AQL_V8 = function (value) {
  'use strict';
  return value;
};

exports.reload = reloadUserFunctions;
