/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, fail */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'rocksdb.auto-refill-index-caches-on-modify' : 'true',
    'rocksdb.auto-refill-index-caches-on-followers' : 'false',
    'server.statistics' : 'false',
    'foxx.queues' : 'false',
  };
}

const db = require('@arangodb').db;
const jsunity = require('jsunity');
const errors = require('internal').errors;
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const cn = 'UnitTestsCollection';

function AutoRefillIndexCachesOnFollowers() {
  'use strict';

  let c;

  return {
    setUpAll: function() {
      c = db._create(cn, { replicationFactor: 2 });
    },

    tearDown: function() {
      IM.debugClearFailAt('', instanceRole.dbServer);
      try {
        c.remove("test");
      } catch (err) {}
    },

    tearDownAll: function() {
      db._drop(cn);
    },
    
    testInsertDefaultOk: function() {
      IM.debugSetFailAt("RefillIndexCacheOnFollowers::failIfTrue", instanceRole.dbServer);
      // insert should just work
      c.insert({_key: "test"});
      assertEqual(1, c.count());
    },
    
    testInsertRequestedOk: function() {
      IM.debugSetFailAt("RefillIndexCacheOnFollowers::failIfTrue", instanceRole.dbServer);
      // insert should just work
      c.insert({_key: "test"}, { refillIndexCaches: true });
      assertEqual(1, c.count());
    },
    
    testInsertOptOutOk: function() {
      IM.debugSetFailAt("RefillIndexCacheOnFollowers::failIfTrue", instanceRole.dbServer);
      // insert should just work
      c.insert({_key: "test"}, { refillIndexCaches: false });
      assertEqual(1, c.count());
    },
    
  };
}

jsunity.run(AutoRefillIndexCachesOnFollowers);
return jsunity.done();
