/*jshint globalstrict:false, strict:false */
/*global assertEqual, fail */

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
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// We
// - spawn a sub-shell
// - wait for it to reach a certain part of the code by checking for a document
// - release it from its breakpoint to continue processing
// - continue our flow to test the operation that could/would conflict

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const sleep = internal.sleep;
const { deriveTestSuite } = require('@arangodb/test-helper');
const {
  launchPlainSnippetInBG,
  joinBGShells,
  cleanupBGShells
} = require('@arangodb/testutils/client-tools').run;

const db = arangodb.db;
const ERRORS = arangodb.errors;
let IM = global.instanceManager;
const cn = "UnitTestsCollection";
const dn = "UnitTestsDB";
const waitFor = IM.options.isInstrumented ? 80 * 7 : 80;

let setupCollection = (type) => {
  let c;
  if (type === 'edge') {
    c = db._createEdgeCollection(cn);
  } else {
    c = db._create(cn);
  }
  let docs = [];
  for (let i = 0; i < 5000; ++i) {
    docs.push({ value1: i, value2: "test" + i, _from: "v/test" + i, _to: "v/test" + i });
  }
  for (let i = 0; i < 350000; i += docs.length) {
    c.insert(docs);
  }
  return c;
};

function BaseTestConfig (dropCb, expectedError) {
  let clients = [];
  return {
    testIndexCreationAborts : function () {
      let c = setupCollection('document');
      dropCb("testIndexCreationAborts");

      try {
        c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
        fail();
      } catch (err) {
        // unfortunately it is possible that this fails because of unexpected
        // scheduling. the whole test is time-based and assumes reasonable
        // scheduling, which probably isn't guaranteed on some CI servers.
        assertEqual(expectedError, err.errorNum);
      }
    },
    
    testIndexCreationInBackgroundAborts : function () {
      let c = setupCollection('document');
      dropCb("testIndexCreationInBackgroundAborts");

      try {
        c.ensureIndex({ type: "persistent", fields: ["value1", "value2"], inBackground: true });
        fail();
      } catch (err) {
        // unfortunately it is possible that this fails because of unexpected
        // scheduling. the whole test is time-based and assumes reasonable
        // scheduling, which probably isn't guaranteed on some CI servers.
        assertEqual(expectedError, err.errorNum);
      }
    },
    
    testWarmupAborts : function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }

      IM.debugSetFailAt("warmup::executeDirectly");
      
      let c = setupCollection('edge');
      dropCb("testWarmupAborts");

      try {
        c.loadIndexesIntoMemory();
        fail();
      } catch (err) {
        // unfortunately it is possible that this fails because of unexpected
        // scheduling. the whole test is time-based and assumes reasonable
        // scheduling, which probably isn't guaranteed on some CI servers.
        assertEqual(expectedError, err.errorNum);
      }
    },

  };
}

function AbortLongRunningOperationsWhenCollectionIsDroppedSuite() {
  'use strict';
  let clients = [];
  let dropCb = (name) => {
    let cn = "UnitTestsCollection";
    clients.push({client: launchPlainSnippetInBG(`
        let cn = "${cn}";
        db[cn].insert({ _key: "runner1", _from: "v/test1", _to: "v/test2" });

        while (!db[cn].exists("runner2")) {
          require("internal").sleep(0.02);
        }

        require("internal").sleep(0.02);
        db._drop(cn);
      `, name)});

    while (!db[cn].exists("runner1")) {
      sleep(0.02);
    }
    db[cn].insert({ _key: "runner2", _from: "v/test1", _to: "v/test2" });
  };

  let suite = {
    setup: function() {
      clients = [];
    },
    tearDown: function () {
      joinBGShells(IM.options, clients, waitFor, cn);
      IM.debugClearFailAt();
      db._drop(cn);
    }
  };

  deriveTestSuite(BaseTestConfig(dropCb, ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code), suite, '_collection');
  return suite;
}

function AbortLongRunningOperationsWhenDatabaseIsDroppedSuite() {
  'use strict';
  let clients = [];

  let dropCb = (name) => {
    let cn = "UnitTestsCollection";
    let old = db._name();
    clients.push({client: launchPlainSnippetInBG(`
          let cn = "${cn}";
          let dn = "${old}";
          db._useDatabase(dn);
          db[cn].insert({ _key: "runner1", _from: "v/test1", _to: "v/test2" });

          while (!db[cn].exists("runner2")) {
            console.log(".");
            require("internal").sleep(0.02);
          }

          require("internal").sleep(0.02);
          db._useDatabase('_system');
          db._dropDatabase(dn);
        `, name)});

    sleep(0.02);
    try {
      while (!db[cn].exists("runner1")) {
        console.log(",");
        sleep(0.02);
      }
      db[cn].insert({ _key: "runner2", _from: "v/test1", _to: "v/test2" });
      db._useDatabase(old);
    } catch (err) {
      db._useDatabase(old);
      throw err;
    }
  };

  let suite = {
    setUp: function () {
      clients = [];
      db._createDatabase(dn);
      db._useDatabase(dn);
    },
    tearDown: function () {
      joinBGShells(IM.options, clients, waitFor, cn);
      
      db._useDatabase('_system');
      IM.debugClearFailAt();
      try {
        db._dropDatabase(dn);
      } catch (err) {
        // in most cases the DB will already have been deleted
      }
    }
  };

  deriveTestSuite(BaseTestConfig(dropCb, ERRORS.ERROR_ARANGO_DATABASE_NOT_FOUND.code), suite, '_database');
  return suite;
}

jsunity.run(AbortLongRunningOperationsWhenCollectionIsDroppedSuite);
jsunity.run(AbortLongRunningOperationsWhenDatabaseIsDroppedSuite);
return jsunity.done();
