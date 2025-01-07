/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertNull, assertTrue, assertFalse */

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
// / @author Michael Hackstein
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////


var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');


if (runSetup === true) {
  db._drop('UnitTestsRecovery');
  db._create('UnitTestsRecovery');

  try {
    db._create('UnitTestsRecovery');
  } catch (e) {
    // This intentionally should fail!
    if (internal.errors.ERROR_ARANGO_DUPLICATE_NAME.code === e.errorNum) {
      // Only this is a valid return code from the server
      return 0;
    }
  }
  // Fail if we get here. We somehow managed to save the same collection twice without error
  return 1;
};


function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testCollectionDuplicateName: function () {
      var c = db._collection('UnitTestsRecovery');
      assertTrue(c !== null && c !== undefined);
    }
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
