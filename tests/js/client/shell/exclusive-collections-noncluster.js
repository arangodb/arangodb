/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue */

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
/// @author Jan Christoph Uhde
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var db = arangodb.db;
const {
  launchPlainSnippetInBG,
  joinBGShells,
} = require('@arangodb/testutils/client-tools').run;

var ERRORS = arangodb.errors;

let IM = global.instanceManager;
const waitFor = IM.options.isInstrumented ? 80 * 7 : 80;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ExclusiveSuite () {
  var cn1 = "UnitTestsExclusiveCollection1"; // used for test data
  var cn2 = "UnitTestsExclusiveCollection2"; // used for communication
  var c1, c2;
  let clients = [];

  return {

    setUp : function () {
      clients = [];
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);
    },

    tearDown : function () {
      joinBGShells(IM.options, clients, waitFor, cn1);
      db._drop(cn1);
      db._drop(cn2);
    },

    testExclusiveExpectConflict : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      clients.push(launchPlainSnippetInBG(`
        let db = require("internal").db;
        db["${cn2}"].insert({ _key: "runner1", value: false });

        while (!db["${cn2}"].exists("runner2")) {
          require("internal").sleep(0.02);
        }

        const trx = db._createTransaction({
          collections: { write: ["${cn1}", "${cn2}"] }
        });
        try {
          for (let i = 0; i < 10; ++i) {
            trx.collection("${cn1}").update("XXX", { name: "runner1" });
          }
          trx.collection("${cn2}").update("runner1", { value: true });
          trx.commit();
        } catch (e) {
          trx.abort();
          // CONFLICT is expected - the main transaction may have committed first
        }
      `, 'testExclusiveExpectConflict'));

      db[cn2].insert({ _key: "runner2", value: false });
      while (!db[cn2].exists("runner1")) {
        require("internal").sleep(0.02);
      }

      try {
        const trx = db._createTransaction({
          collections: { write: [ cn1, cn2 ] }
        });
        try {
          for (let i = 0; i < 10; ++i) {
            trx.collection(cn1).update("XXX", { name: "runner2" });
          }
          trx.collection(cn2).update("runner2", { value: true });
          trx.commit();
        } catch (e) {
          trx.abort();
          throw e;
        }
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      joinBGShells(IM.options, clients, waitFor, cn1);
      clients = [];

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertNotEqual(docs[0].value, docs[1].value);
    },

    testExclusiveExpectNoConflict : function () {
      assertEqual(0, c2.count());
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      clients.push(launchPlainSnippetInBG(`
        let db = require("internal").db;
        db["${cn2}"].insert({ _key: "runner1", value: false });
        while (!db["${cn2}"].exists("runner2")) {
          require("internal").sleep(0.02);
        }

        const trx = db._createTransaction({
          collections: { exclusive: ["${cn1}", "${cn2}"] }
        });
        try {
          for (let i = 0; i < 10; ++i) {
            trx.collection("${cn1}").update("XXX", { name: "runner1" });
          }
          trx.collection("${cn2}").update("runner1", { value: true });
          trx.commit();
        } catch (e) {
          trx.abort();
          throw e;
        }
      `, 'testExclusiveExpectNoConflict'));

      db[cn2].insert({ _key: "runner2", value: false });
      while (!db[cn2].exists("runner1")) {
        require("internal").sleep(0.02);
      }

      const trx = db._createTransaction({
        collections: { exclusive: [ cn1, cn2 ] }
      });
      try {
        for (let i = 0; i < 10; ++i) {
          trx.collection(cn1).update("XXX", { name: "runner2" });
        }
        trx.collection(cn2).update("runner2", { value: true });
        trx.commit();
      } catch (e) {
        trx.abort();
        throw e;
      }

      joinBGShells(IM.options, clients, waitFor, cn1);
      clients = [];

      // both transactions should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertTrue(docs[0].value);
      assertTrue(docs[1].value);
    },

    testExclusiveExpectConflictAQL : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      clients.push(launchPlainSnippetInBG(`
        let db = require("internal").db;
        db["${cn2}"].insert({ _key: "runner1", value: false });
        while (!db["${cn2}"].exists("runner2")) {
          require("internal").sleep(0.02);
        }
        try {
          for (let i = 0; i < 10000; ++i) {
            db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner1' } UPDATE { name: 'runner1' } IN ${cn1}");
          }
          db["${cn2}"].update("runner1", { value: true });
        } catch (e) {
          // CONFLICT is expected
        }
      `, 'testExclusiveExpectConflictAQL'));

      try {
        db[cn2].insert({ _key: "runner2", value: false });
        while (!db[cn2].exists("runner1")) {
          require("internal").sleep(0.02);
        }
        for (let i = 0; i < 10000; i++) {
          db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner2' } UPDATE { name: 'runner2' } IN " + cn1);
        }
        db[cn2].update("runner2", { value: true });
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      joinBGShells(IM.options, clients, waitFor, cn1);
      clients = [];

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertNotEqual(docs[0].value, docs[1].value);
    },

    testExclusiveExpectNoConflictAQL : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      clients.push(launchPlainSnippetInBG(`
        let db = require("internal").db;
        db["${cn2}"].insert({ _key: "runner1", value: false });
        while (!db["${cn2}"].exists("runner2")) {
          require("internal").sleep(0.02);
        }
        for (let i = 0; i < 10000; ++i) {
          db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner1' } UPDATE { name: 'runner1' } IN ${cn1} OPTIONS { exclusive: true }");
        }
        db["${cn2}"].update("runner1", { value: true });
      `, 'testExclusiveExpectNoConflictAQL'));

      db[cn2].insert({ _key: "runner2", value: false });
      while (!db[cn2].exists("runner1")) {
        require("internal").sleep(0.02);
      }
      for (let i = 0; i < 10000; i++) {
        db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner2' } UPDATE { name: 'runner2' } IN " + cn1 + " OPTIONS { exclusive: true }");
      }
      db[cn2].update("runner2", { value: true });

      joinBGShells(IM.options, clients, waitFor, cn1);
      clients = [];

      // both transactions should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertTrue(docs[0].value);
      assertTrue(docs[1].value);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ExclusiveSuite);

return jsunity.done();
