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

  validateName(name);

  return db._connection.DELETE('/_api/aqlfunction/' + encodeURIComponent(name), 'exactMatch=true');
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

  return db._connection.DELETE('/_api/aqlfunction/' + encodeURIComponent(group), 'exactMatch=false');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsRegister
// //////////////////////////////////////////////////////////////////////////////

var registerFunction = function (name, code, isDeterministic = false) {
  var db = internal.db;

  var requestResult = db._connection.POST('/_api/aqlfunction/' + encodeURIComponent(name),
                                          JSON.stringify({
                                            name: name,
                                            code: stringifyFunction(code),
                                            isDeterministic: isDeterministic
                                          }));

  // TODO arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsToArray
// //////////////////////////////////////////////////////////////////////////////

var toArrayFunctions = function (group) {
  'use strict';

  var requestResult = db._connection.GET('/_api/aqlfunction/' + encodeURIComponent(group), '');

  return requestResult;
};

exports.unregister = unregisterFunction;
exports.unregisterGroup = unregisterFunctionsGroup;
exports.register = registerFunction;
exports.toArray = toArrayFunctions;
