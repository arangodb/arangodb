/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, getOptions, fail, assertFalse, assertMatch */

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
/// @author Copyright 2019, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'rocksdb.exclusive-writes': 'false',
  };
}

const jsunity = require("jsunity");
const {
  launchPlainSnippetInBG,
  joinBGShells,
} = require('@arangodb/testutils/client-tools').run;
const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;

let IM = global.instanceManager;
const waitFor = IM.options.isInstrumented ? 80 * 7 : 80;

function OptionsTestSuite () {
  const cn1 = "UnitTestsExclusiveCollection1"; // used for test data
  const cn2 = "UnitTestsExclusiveCollection2"; // used for communication
  let c1, c2;
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

    testExclusiveExpectConflictWithoutOption : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      clients.push(launchPlainSnippetInBG(`
        const arangodb = require("@arangodb");
        const ERRORS = arangodb.errors;
        let db = require("internal").db;
        db["${cn2}"].insert({ _key: "runner1", value: false });

        while (!db["${cn2}"].exists("runner2")) {
          require("internal").sleep(0.02);
        }

        try {
          for (let i = 0; i < 50; ++i) {
            const trx = db._createTransaction({ collections: { write: ["${cn1}"] } });
            try {
              const tc = trx.collection("${cn1}");
              for (let j = 0; j < 10; ++j) {
                tc.update("XXX", { name: "runner1" });
              }
              trx.commit();
            } catch (e) {
              trx.abort();
              throw e;
            }

            if (db["${cn2}"].document("runner2").value) {
              break;
            }
          }
        } catch (err) {
          db["${cn2}"].insert({ _key: "other-failed", errorNum: err.errorNum, errorMessage: err.errorMessage, code: err.code });
        }
        db["${cn2}"].update("runner1", { value: true });
      `, 'testExclusiveExpectConflictWithoutOption'));

      db[cn2].insert({ _key: "runner2", value: false });
      while (!db[cn2].exists("runner1")) {
        require("internal").sleep(0.02);
      }

      try {
        for (let i = 0; i < 50; ++i) {
          const trx = db._createTransaction({ collections: { write: [ cn1 ] } });
          try {
            const tc = trx.collection(cn1);
            for (let j = 0; j < 10; ++j) {
              tc.update("XXX", { name: "runner2" });
            }
            trx.commit();
          } catch (e) {
            trx.abort();
            throw e;
          }

          if (db[cn2].document("runner1").value) {
            break;
          }
        }
      } catch (err) {
        db[cn2].insert({ _key: "we-failed", errorNum: err.errorNum, errorMessage: err.errorMessage, code: err.code });
      }
      db[cn2].update("runner2", { value: true });

      joinBGShells(IM.options, clients, waitFor, cn1);
      clients = [];

      let found = 0;
      let keys = ["other-failed", "we-failed"];
      keys.forEach((k) => {
        if (db[cn2].exists(k)) {
          let doc = db[cn2].document(k);
          assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, doc.errorNum);
          ++found;
          db[cn2].remove(k);
        }
      });

      assertEqual(1, found);

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
    },

    testExclusiveExpectNoConflicts : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      clients.push(launchPlainSnippetInBG(`
        const arangodb = require("@arangodb");
        const ERRORS = arangodb.errors;
        let db = require("internal").db;
        db["${cn2}"].insert({ _key: "runner1", value: false });

        while (!db["${cn2}"].exists("runner2")) {
          require("internal").sleep(0.02);
        }

        try {
          for (let i = 0; i < 50; ++i) {
            for (let j = 0; j < 200; ++j) {
              try {
                db["${cn1}"].update("XXX", { name: "runner1" });
              } catch (e) {
                if (e.errorNum !== ERRORS.ERROR_ARANGO_CONFLICT.code) {
                  throw e;
                }
                j--;
              }
            }

            if (db["${cn2}"].document("runner2").value) {
              break;
            }
          }
        } catch (err) {
          db["${cn2}"].insert({ _key: "other-failed", errorNum: err.errorNum, errorMessage: err.errorMessage, code: err.code });
        }
        db["${cn2}"].update("runner1", { value: true });
      `, 'testExclusiveExpectNoConflicts'));

      db[cn2].insert({ _key: "runner2", value: false });
      while (!db[cn2].exists("runner1")) {
        require("internal").sleep(0.02);
      }

      try {
        for (let i = 0; i < 50; ++i) {
          for (let j = 0; j < 200; ++j) {
            try {
              db[cn1].update("XXX", { name: "runner2" });
            } catch (e) {
              if (e.errorNum !== ERRORS.ERROR_ARANGO_CONFLICT.code) {
                throw e;
              }
              j--;
            }
          }
          if (db[cn2].document("runner1").value) {
            break;
          }
        }
      } catch (err) {
        db[cn2].insert({ _key: "we-failed", errorNum: err.errorNum, errorMessage: err.errorMessage, code: err.code });
      }
      db[cn2].update("runner2", { value: true });

      joinBGShells(IM.options, clients, waitFor, cn1);
      clients = [];

      let keys = ["other-failed", "we-failed"];
      let errors = [];
      keys.forEach((k) => {
        if (db[cn2].exists(k)) {
          let doc = db[cn2].document(k);
          errors.push(doc);
          db[cn2].remove(k);
        }
      });

      assertEqual(0, errors.length, "Found errors: " + JSON.stringify(errors));

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
    }
  };
}

jsunity.run(OptionsTestSuite);

return jsunity.done();
