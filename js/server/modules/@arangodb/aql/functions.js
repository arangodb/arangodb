/* jshint strict: false */
/* global UNREGISTER_AQL_USER_FUNCTION, UNREGISTER_AQL_USER_FUNCTION_GROUP, REGISTER_AQL_USER_FUNCTION, GET_AQL_USER_FUNCTIONS */

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsUnregister
// //////////////////////////////////////////////////////////////////////////////

var unregisterFunction = function (name) {
  'use strict';

  if (UNREGISTER_AQL_USER_FUNCTION(name)) {
    require('@arangodb/aql').reload();
    return true;
  } else {
    return false;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsUnregisterGroup
// //////////////////////////////////////////////////////////////////////////////

var unregisterFunctionsGroup = function (group) {
  'use strict';
  var deleted = UNREGISTER_AQL_USER_FUNCTION_GROUP(group);

  if (deleted > 0) {
    require('@arangodb/aql').reload();
  }

  return deleted;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock aqlFunctionsRegister
// //////////////////////////////////////////////////////////////////////////////

var registerFunction = function (name, code, isDeterministic) {
  'use strict';
  if (typeof isDeterministic !== 'boolean') {
   isDeterministic = false;
  }
  if (typeof code === 'function') {
    code = String(code) + '\n';
  }

  let result = REGISTER_AQL_USER_FUNCTION({
    name: name,
    code: code,
    isDeterministic: isDeterministic
  });
  require('@arangodb/aql').reload();

  return result;
};

exports.unregister = unregisterFunction;
exports.unregisterGroup = unregisterFunctionsGroup;
exports.register = registerFunction;
exports.toArray = GET_AQL_USER_FUNCTIONS;
