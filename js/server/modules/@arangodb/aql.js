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

// / @brief user functions cache
var UserFunctions = { };

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

// / @brief return a database-specific function prefix
function DB_PREFIX () {
  return INTERNAL.db._name();
}

// / @brief reset the user functions and reload them from the database
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

exports.reload = reloadUserFunctions;
