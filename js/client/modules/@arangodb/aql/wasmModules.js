/* jshint strict: false */
/* global db */

// //////////////////////////////////////////////////////////////////////////////
// / @brief AQL WebAssembly user functions management
// /
// / @file
// /
// / DISCLAIMER
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Julia Volmer
// / @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
var internal = require('internal');
var arangosh = require('@arangodb/arangosh');
var fs = require('fs');

// //////////////////////////////////////////////////////////////////////////////
// / @brief encode data in file with base64 
// //////////////////////////////////////////////////////////////////////////////

var base64 = function (file) {
    var buffer = fs.readFileSync(file);
    return buffer.toString('base64');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock delete a user-defined webassembly module by name
// //////////////////////////////////////////////////////////////////////////////

var unregisterModule = function (name) {
  var requestResult = db._connection.DELETE('/_api/wasm/' + name);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock add a user-defined webassembly module.
//                        codefile is the path to the webassembly code file
// //////////////////////////////////////////////////////////////////////////////

var registerModuleByFile = function (name, codefile, isDeterministic = false) {
  var requestResult = db._connection.POST('/_api/wasm/',
                                          {
                                            name: name,
                                            code: base64(codefile),
                                            isDeterministic: isDeterministic
                                          });
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock add a user-defined webassembly module.
//                        codefile is the path to the webassembly code file
// //////////////////////////////////////////////////////////////////////////////

var registerModuleByBase64 = function (name, base64code, isDeterministic = false) {
  var requestResult = db._connection.POST('/_api/wasm/',
                                          {
                                            name: name,
                                            code: base64code,
                                            isDeterministic: isDeterministic
                                          });
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock get all user-defined webassembly modules
// //////////////////////////////////////////////////////////////////////////////

var toArrayModules = function () {
  var requestResult = db._connection.GET('/_api/wasm/');
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock show detail information of user-defined webassembly module
// //////////////////////////////////////////////////////////////////////////////

var showModule = function (name) {
  var requestResult = db._connection.GET('/_api/wasm/' + name);
  arangosh.checkRequestResult(requestResult);
  return requestResult.result;
}

exports.unregister = unregisterModule;
exports.registerByFile = registerModuleByFile;
exports.register = registerModuleByBase64;
exports.toArray = toArrayModules;
exports.show = showModule;
