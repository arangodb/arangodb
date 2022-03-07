/* jshint strict: false */
/* global db */

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
// / @author Julia Volmer
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');

var ArangoError = require('@arangodb').ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief eventually convert function to string
// //////////////////////////////////////////////////////////////////////////////

var stringifyFunction = function (code, name) {
  'use strict';

  if (typeof code === 'function') {
    code = String(code) + '\n';
  }
  return code;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock wasmFunctionsUnregister
// //////////////////////////////////////////////////////////////////////////////

var unregisterFunction = function (name) {
  'use strict';

  var requestResult = db._connection.DELETE('/_api/wasm/', { name });

  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock wasmFunctionsRegister
// //////////////////////////////////////////////////////////////////////////////

var registerFunction = function (name, code, isDeterministic = false) {
  var db = internal.db;

  var requestResult = db._connection.POST('/_api/wasm/',
                                          {
                                            name: name,
                                            code: stringifyFunction(code),
                                            isDeterministic: isDeterministic
                                          });
  
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock wasmFunctionsToArray
// //////////////////////////////////////////////////////////////////////////////

var toArrayFunctions = function () {
  'use strict';

    var requestResult = db._connection.GET('/_api/wasm/');

  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

exports.unregister = unregisterFunction;
exports.register = registerFunction;
exports.toArray = toArrayFunctions;
