/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertUndefined */

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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let env = require('process').env;
if (getOptions === true) {
  env['PATHTT'] = 'denylist';
  return {
    'javascript.environment-variables-denylist': 'PATH',
    'javascript.environment-variables-allowlist': '^MIAU-DERFUCHS$|PATHTT|^INSTANCEINFO$'
  };
}

let jsunity = require('jsunity');

function testSuite() {
  return {
    setUp: function() {},
    tearDown: function() {},
    
    testAvailable : function() {
      assertFalse(env.hasOwnProperty('MIAU-DERFUCHS'));
      assertUndefined(env['MIAU-DERFUCHS']);

      env['MIAU-DERFUCHS'] = 'DERFUCHS';
      assertTrue(env.hasOwnProperty('MIAU-DERFUCHS'));
      assertEqual('DERFUCHS', env['MIAU-DERFUCHS']);

      env['MIAU-DERFUCHS'] = 'KATZ!';
      assertTrue(env.hasOwnProperty('MIAU-DERFUCHS'));
      assertEqual('KATZ!', env['MIAU-DERFUCHS']);

      delete env['MIAU-DERFUCHS'];
      assertFalse(env.hasOwnProperty('MIAU-DERFUCHS'));
    },

    testMasked : function() {
      // the PATH was hidden by the parameter
      assertUndefined(env['PATH']);
      assertFalse(env.hasOwnProperty('PATH'));

      // should not overwrite it, and still hide it 
      env['PATH'] = 'what?';
      assertUndefined(env['PATH']);
      assertFalse(env.hasOwnProperty('PATH'));
      
      // the test env always has the INSTANCEINFO
      assertTrue(env.hasOwnProperty('INSTANCEINFO'));
      assertTrue(env['INSTANCEINFO'].length > 0);

      assertTrue(env.hasOwnProperty('PATHTT'));
      assertEqual(env['PATHTT'], 'denylist');
      
    }
  };
}
jsunity.run(testSuite);
return jsunity.done();
