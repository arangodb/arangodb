/* jshint strict: false, unused: true, bitwise: false, esnext: true */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const INTERNAL = require('internal');
const ArangoError = require('@arangodb').ArangoError;

// / @brief user functions cache
let UserFunctions = { };

// / @brief throw a runtime exception
function THROW (func, error, data, moreMessage) {
  'use strict';

  let prefix = '';
  if (func !== null && func !== '') {
    prefix = "in function '" + func + "()': ";
  }

  let err = new ArangoError();

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

// / @brief return a database-specific function prefix
function DB_PREFIX () {
  return INTERNAL.db._name();
}

// / @brief reset the user functions and reload them from the database
function reloadUserFunctions () {
  'use strict';

  const prefix = DB_PREFIX();
  let c = INTERNAL.db._collection('_aqlfunctions');

  if (c === null) {
    // collection not found. now reset all user functions
    UserFunctions = { };
    UserFunctions[prefix] = { };
    return;
  }

  let foundError = false;
  let functions = {};

  c.toArray().forEach(function (f) {
    let key = f._key.replace(/:{1,}/g, '::');
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

// reload a single AQL user-defined function by name
function reloadUserFunction (name) {
  'use strict';

  let prefix = DB_PREFIX();
  let c = INTERNAL.db._collection('_aqlfunctions');

  if (c === null) {
    UserFunctions = {};
    return;
  }

  if (!UserFunctions.hasOwnProperty(prefix)) {
    UserFunctions[prefix] = {};
  }

  let key = name.replace(/:{1,}/g, '::').toUpperCase();

  try {
    // use the single-document GET operation here, which does not require a full
    // AQL query setup and is much cheaper and less error-prone than a full AQL
    // query.
    let doc = c.document(key);
    let code;
    if (doc.code.match(/^\(?function\s+\(/)) {
      code = doc.code;
    } else {
      code = '(function() { var callback = ' + doc.code + ';\n return callback; })();';
    }

    try {
      let res = INTERNAL.executeScript(code, undefined, '(user function ' + key + ')');
      if (typeof res !== 'function') {
        THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_INVALID_CODE);
      }
      UserFunctions[prefix][key] = {
        name: key,
        func: res,
        isDeterministic: doc.isDeterministic || false
      };
    } catch (err) {
      THROW(null, INTERNAL.errors.ERROR_QUERY_FUNCTION_INVALID_CODE);
    }
  } catch (err) {
  }
}

// / @brief box a value into the AQL datatype system
function FIX_VALUE (value) {
  'use strict';

  let type = typeof (value);

  if (value === undefined ||
    value === null ||
    (type === 'number' && (isNaN(value) || !isFinite(value)))) {
    return null;
  }

  if (type === 'boolean' || type === 'string' || type === 'number') {
    return value;
  }

  if (Array.isArray(value)) {
    const n = value.length;
    for (let i = 0; i < n; ++i) {
      value[i] = FIX_VALUE(value[i]);
    }

    return value;
  }

  if (type === 'object') {
    let result = {};

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

  let prefix = DB_PREFIX(), reloaded = false;
  if (!UserFunctions.hasOwnProperty(prefix)) {
    // gracefully only reload the one missing function
    reloadUserFunction(name);
    reloaded = true;
  }

  if (!UserFunctions[prefix].hasOwnProperty(name) && !reloaded) {
    // last chance
    // gracefully only reload the one missing function
    reloadUserFunction(name);
  }

  if (!UserFunctions[prefix].hasOwnProperty(name)) {
    THROW(func, INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, name);
  }

  try {
    return FIX_VALUE(UserFunctions[prefix][name].func.apply({ name: name }, parameters));
  } catch (err) {
    THROW(name, INTERNAL.errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, String(err.stack || String(err)));
    return null;
  }
};

// / @brief passthru the argument
// / this function is marked as non-deterministic so its argument withstands
// / query optimisation. this function can be used for testing
exports.AQL_V8 = function (value) {
  'use strict';
  return value;
};

// reload all AQL user-defined functions for the current database
exports.reload = reloadUserFunctions;

// flush the cache for user-defined functions for all databases.
// not meant to be part of the public API
exports.flush = () => { UserFunctions = {}; };
