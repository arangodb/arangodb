/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual */

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

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  var c, i;
  db._drop('UnitTestsRecovery');
  c = db._create('UnitTestsRecovery', { 'id': "9999990" });
  for (i = 0; i < 10; ++i) {
    c.save({ _key: 'test' + i, value1: i, value2: 'test', value3: [ 'foo', i ] });
  }

  // drop the collection
  c.drop();
  internal.wait(5);

  c = db._create('UnitTestsRecovery', { 'id': "9999990" });
  for (i = 0; i < 10; ++i) {
    c.save({ _key: 'test' + i, value3: i, value1: 'test', value2: [ 'foo', i ] });
  }

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testCollectionsDifferentAttributes: function () {
      var c, i, doc;
      c = db._collection('UnitTestsRecovery');

      for (i = 0; i < 10; ++i) {
        doc = c.document('test' + i);
        assertEqual(i, doc.value3);
        assertEqual('test', doc.value1);
        assertEqual([ 'foo', i ], doc.value2);
      }
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
