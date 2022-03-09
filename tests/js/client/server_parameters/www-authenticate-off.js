/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'false'
  };
}
const jsunity = require('jsunity');
const request = require('@arangodb/request').request;

function testSuite() {
  return {
    testHeader : function() {
      let result = request({ url: "/_api/version", method: "get" });
      assertEqual(200, result.status);
      assertFalse(result.headers.hasOwnProperty('www-authenticate'));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
