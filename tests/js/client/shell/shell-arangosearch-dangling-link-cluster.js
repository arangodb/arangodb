/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertEqual */
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
/// @author Andrei Lobov
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const internal = require('internal');
let IM = global.instanceManager;

function ArangoSearchDanglingLinkSuite () {
  return {
    setUpAll : function () {
      IM.debugClearFailAt();
      db._useDatabase('_system');
      db._createDatabase('UnitTestsDangling');
      db._useDatabase('UnitTestsDangling');
      db._create("foo");
      db.foo.save({a:1, b:'text'});
    },
    
    setUp : function() {
      IM.debugClearFailAt();
    },
    
    tearDownAll : function () {
      IM.debugClearFailAt();
      db._useDatabase('_system');
      db._dropDatabase('UnitTestsDangling');
    },

    testDanglingLink : function () {
      IM.debugSetFailAt("IResearchLink::alwaysDangling");
      db._createView("dangle", "arangosearch", {links:{foo:{includeAllFields:true}}});
      db._dropView("dangle");
      let nCount = 0;
      while(db.foo.getIndexes(true, true).length > 1) {
        internal.sleep(1);
        nCount++;
        // 30 secs should be more than enough to kick in cleanup
        assertTrue(nCount < 30);
      }
    },
    
    testDanglingLinkRetryDrop : function () {
      IM.debugSetFailAt("IResearchLink::alwaysDangling");
      IM.debugSetFailAt("IResearchLink::failDropDangling");
      db._createView("dangle", "arangosearch", {links:{foo:{includeAllFields:true}}});
      db._dropView("dangle");
      let nCount = 0;
      while(db.foo.getIndexes(true, true).length > 1 && nCount < 10) {
        internal.sleep(1);
        nCount++;
      }
      assertEqual(10, nCount);
      IM.debugClearFailAt("IResearchLink::failDropDangling");
      while(db.foo.getIndexes(true, true).length > 1) {
        internal.sleep(1);
        nCount++;
        // 30 secs should be more than enough to kick in cleanup
        assertTrue(nCount < 30);
      } 
    },

  };
}

if (IM.debugCanUseFailAt()) {
  jsunity.run(ArangoSearchDanglingLinkSuite);
}

return jsunity.done();
