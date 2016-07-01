/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual */

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
var fs = require('fs');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  var i, paths = [];
  for (i = 0; i < 3; ++i) {
    db._useDatabase('_system');
    try {
      db._dropDatabase('UnitTestsRecovery' + i);
    } catch (err) {}

    db._createDatabase('UnitTestsRecovery' + i);
    db._useDatabase('UnitTestsRecovery' + i);
    paths[i] = db._path();

    db._useDatabase('_system');
    db._dropDatabase('UnitTestsRecovery' + i);
  }

  db._drop('test');
  var c = db._create('test');
  c.save({ _key: 'crashme' }, true);

  internal.wait(3, true);
  for (i = 0; i < 3; ++i) {
    // re-create database directory
    try {
      fs.makeDirectory(paths[i]);
    } catch (err2) {}

    fs.write(fs.join(paths[i], 'parameter.json'), '');
    fs.write(fs.join(paths[i], 'parameter.json.tmp'), '');

    // create some collection directory
    fs.makeDirectory(fs.join(paths[i], 'collection-123'));
    if (i < 2) {
      // create a leftover parameter.json.tmp file
      fs.write(fs.join(paths[i], 'collection-123', 'parameter.json.tmp'), '');
    } else {
      // create an empty parameter.json file
      fs.write(fs.join(paths[i], 'collection-123', 'parameter.json'), '');
      // create some other file
      fs.write(fs.join(paths[i], 'collection-123', 'foobar'), 'foobar');
    }
  }

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
    // / @brief test whether we can recover the databases even with the
    // / leftover directories present
    // //////////////////////////////////////////////////////////////////////////////

    testLeftoverDatabaseDirectory: function () {
      assertEqual([ '_system' ], db._databases());
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
