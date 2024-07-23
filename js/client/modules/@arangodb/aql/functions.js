/* jshint strict: false */
/* global db */

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

var unregisterFunction = function (name) {
  'use strict';

  var requestResult = db._connection.DELETE('/_api/aqlfunction/' + encodeURIComponent(name) + '?group=false');
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

var unregisterFunctionsGroup = function (group) {
  'use strict';

  return db._connection.DELETE('/_api/aqlfunction/' + encodeURIComponent(group) + '?group=true');
};

var registerFunction = function (name, code, isDeterministic = false) {
  var db = internal.db;

  var requestResult = db._connection.POST('/_api/aqlfunction/' + encodeURIComponent(name),
                                          {
                                            name: name,
                                            code: stringifyFunction(code),
                                            isDeterministic: isDeterministic
                                          });

  arangosh.checkRequestResult(requestResult);
  return !requestResult.isNewlyCreated;
};

var toArrayFunctions = function (group) {
  'use strict';

  var requestResult = db._connection.GET('/_api/aqlfunction/' + encodeURIComponent(group), '');

  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

exports.unregister = unregisterFunction;
exports.unregisterGroup = unregisterFunctionsGroup;
exports.register = registerFunction;
exports.toArray = toArrayFunctions;
