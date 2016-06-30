/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief AQL user functions management
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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

var internal = require('internal');
var arangodb = require('@arangodb');

var db = arangodb.db;
var ArangoError = arangodb.ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the _aqlfunctions collection
// //////////////////////////////////////////////////////////////////////////////

var getStorage = function () {
  'use strict';

  var functions = db._collection('_aqlfunctions');

  if (functions === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = "collection '_aqlfunctions' not found";

    throw err;
  }

  return functions;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply a prefix filter on the functions
// //////////////////////////////////////////////////////////////////////////////

var getFiltered = function (group) {
  'use strict';

  var result = [];

  if (group !== null && group !== undefined && group.length > 0) {
    var prefix = group.toUpperCase();

    if (group.length > 1 && group.substr(group.length - 2, 2) !== '::') {
      prefix += '::';
    }

    getStorage().toArray().forEach(function (f) {
      if (f.name.toUpperCase().substr(0, prefix.length) === prefix) {
        result.push(f);
      }
    });
  } else {
    result = getStorage().toArray();
  }

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief validate a function name
// //////////////////////////////////////////////////////////////////////////////

var validateName = function (name) {
  'use strict';

  if (typeof name !== 'string' ||
    !name.match(/^[a-zA-Z0-9_]+(::[a-zA-Z0-9_]+)+$/) ||
    name.substr(0, 1) === '_') {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.message;

    throw err;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief validate user function code
// //////////////////////////////////////////////////////////////////////////////

var stringifyFunction = function (code, name) {
  'use strict';

  if (typeof code === 'function') {
    code = String(code) + '\n';
  }

  if (typeof code === 'string') {
    code = '(' + code + '\n)';

    if (!internal.parse) {
      // no parsing possible. assume always valid
      return code;
    }

    try {
      if (internal.parse(code, name)) {
        // parsing successful
        return code;
      }
    } catch (e) {}
  }

  // fall-through intentional

  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code;
  err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.message;

  throw err;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsUnregister
// //////////////////////////////////////////////////////////////////////////////

var unregisterFunction = function (name) {
  'use strict';

  var func = null;

  validateName(name);

  try {
    func = getStorage().document(name.toUpperCase());
  } catch (err1) {}

  if (func === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code;
    err.errorMessage = internal.sprintf(arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.message, name);

    throw err;
  }

  getStorage().remove(func._id);
  internal.reloadAqlFunctions();

  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsUnregisterGroup
// //////////////////////////////////////////////////////////////////////////////

var unregisterFunctionsGroup = function (group) {
  'use strict';

  if (group.length === 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message;

    throw err;
  }

  var deleted = 0;

  getFiltered(group).forEach(function (f) {
    getStorage().remove(f._id);
    deleted++;
  });

  if (deleted > 0) {
    internal.reloadAqlFunctions();
  }

  return deleted;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsRegister
// //////////////////////////////////////////////////////////////////////////////

var registerFunction = function (name, code, isDeterministic) {
  // validate input
  validateName(name);

  code = stringifyFunction(code, name);

  var testCode = '(function() { var callback = ' + code + '; return callback; })()';
  var err;

  try {
    if (internal && internal.hasOwnProperty('executeScript')) {
      var evalResult = internal.executeScript(testCode, undefined, '(user function ' + name + ')');
      if (typeof evalResult !== 'function') {
        err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code;
        err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.message +
          ': code must be contained in function';
        throw err;
      }
    }
  } catch (err1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.message;
    throw err;
  }

  var result = db._executeTransaction({
    collections: {
      write: getStorage().name()
    },
    action: function (params) {
      var exists = false;
      var collection = require('internal').db._collection(params.collection);
      var name = params.name;

      try {
        collection.remove(name.toUpperCase());
        exists = true;
      } catch (err2) {}

      var data = {
        _key: name.toUpperCase(),
        name: name,
        code: params.code,
        isDeterministic: params.isDeterministic || false
      };

      collection.save(data);
      return exists;
    },
    params: {
      name: name,
      code: code,
      isDeterministic: isDeterministic,
      collection: getStorage().name()
    }
  });

  internal.reloadAqlFunctions();

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsToArray
// //////////////////////////////////////////////////////////////////////////////

var toArrayFunctions = function (group) {
  'use strict';

  var result = [];

  getFiltered(group).forEach(function (f) {
    result.push({ name: f.name, code: f.code.substr(1, f.code.length - 2).trim() });
  });

  return result;
};

exports.unregister = unregisterFunction;
exports.unregisterGroup = unregisterFunctionsGroup;
exports.register = registerFunction;
exports.toArray = toArrayFunctions;
