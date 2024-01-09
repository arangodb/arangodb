/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
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
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.options-api': "disabled"
  };
}

const jsunity = require('jsunity');

function testSuite() {
  return {
    testApiGetOptions : function() {
      let res = arango.GET("/_admin/options");
      assertTrue(res.error);
      assertEqual(404, res.code);
    },

    testApiGetOptionsDescription : function() {
      let res = arango.GET("/_admin/options-description");
      assertTrue(res.error);
      assertEqual(404, res.code);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
