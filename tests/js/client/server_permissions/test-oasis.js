/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail */

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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': 'abc123',

    'javascript.startup-options-blacklist': [ '.*' ],
    'javascript.endpoints-whitelist' : [
      //'ssl://arangodb.com:443'
      'arangodb.com'
    ],
    'javascript.files-whitelist' : [
      '^$'
    ],
    'javascript.environment-variables-whitelist' : [
      '^HOSTNAME$',
      '^PATH$',
    ],
    'javascript.harden' : 'true',
    'server.harden': 'true',
  };
}

const jsunity = require('jsunity');
const internal = require('internal');
const arango = internal.arango;

function testSuite() {

  return {
    testStatisics : function() {
      let result = arango.GET("/_db/_system/_admin/aardvark/statistics/short");
      assertTrue(result.physicalMemory > 100);
      assertEqual(result.enabled, true);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
