/* jshint globalstrict:false, strict:false, unused : false */
/* global assertNull, assertNotNull, assertEqual, fail */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for dump/reload
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');
var ArangoCollection = internal.ArangoCollection;

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecovery');
  var c = db._create('UnitTestsRecovery');
  c.insert({_key: "foo", what: "bar" });
  var path = c.path();
  c.unload();
  c = null;
  internal.wal.flush(true, true);

  while (true) {
    if (db._collection('UnitTestsRecovery').status() === ArangoCollection.STATUS_UNLOADED) {
      break;
    }
    internal.print(db._collection('UnitTestsRecovery').status());
    internal.wait(0.5, true);
  }

  var fs = require("fs");
  // read parameter.json
  var content = fs.readFileSync(fs.join(path, "parameter.json"));
  // store its contents in parameter.json.tmp
  fs.writeFileSync(fs.join(path, "parameter.json.tmp"), content);
  // and remove parameter.json
  fs.remove(fs.join(path, "parameter.json"));

  internal.debugSegfault('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {},
    tearDown: function () {},

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether failures when creating collections causes issues
    // //////////////////////////////////////////////////////////////////////////////

    testCreateCollectionTmpfile: function () {
      var c = db._collection('UnitTestsRecovery');
      assertNotNull(c);
      assertEqual(1, c.count());
      assertEqual("bar", c.document("foo").what);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

function main (argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.done().status ? 0 : 1;
  }
}
