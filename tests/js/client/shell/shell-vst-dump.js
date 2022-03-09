/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global arango, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;

function vstDumpSuite () {
  'use strict';

  const cn = 'UnitTestsCollection';

  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testVstDump: function () {
      db[cn].insert({ _key: "test", value: 1 });

      let doc = arango.GET("/_api/document/" + encodeURIComponent(cn) + "/test", {
        accept: "application/json"
      });
 
      assertEqual("test", doc._key);
      assertEqual(cn + "/test", doc._id);
      assertEqual(1, doc.value);
    },
    
  };
}

if (arango.protocol() === 'vst') {
  jsunity.run(vstDumpSuite);
}

return jsunity.done();
