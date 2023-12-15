/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, print */

////////////////////////////////////////////////////////////////////////////////
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Valery Mironov
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const fs = require("fs");
const db = arangodb.db;
const isCluster = internal.isCluster();
const isServer = require('@arangodb').isServer;

function DropDatabase() {
  let path = fs.join(db._path(), 'databases');
  let was = 0;

  function dirmaker(n, needDrop) {
    db._useDatabase("_system");
    db._createDatabase(n);
    db._useDatabase(n);
    try {
      db._create("c", {numberOfShards: 3});
      db._createView("v", "arangosearch", {});
      db.v.properties({links: {c: {includeAllFields: true}}});
      for (let i = 0; i < 100; ++i) {
        db.c.insert({Hallo: i});
      }
      db._query(`FOR d IN v OPTIONS { waitForSync:true } LIMIT 1 RETURN 1`);
      if (needDrop) {
        db.v.properties({links: {c: null}});
        db._dropView("v");
        db._drop("c");
      }
    } catch (_) {
    }
    db._useDatabase("_system");
    db._dropDatabase(n);
  }

  return {
    setUpAll: function () {
      was = fs.list(path).length;
    },

    tearDown: function () {
      for (let i = 0; i < 20; ++i) {
        let now = fs.list(path).length;
        if (was === now) {
          return;
        }
        internal.sleep(0.5);
      }
      print(path);
      print(fs.list(path));
      assertTrue(false);
    },

    testWithDropView: function () {
      for (let i = 0; i < 5; ++i) {
        dirmaker("d" + i, true);
      }
    },
    testWithoutDropView: function () {
      for (let i = 0; i < 5; ++i) {
        dirmaker("d" + i, false);
      }
    },
  };
}

if (!isCluster && isServer) {
  jsunity.run(DropDatabase);
} else {
  // TODO(MBkkt) Add test for cluster, we need to know path to dbserver
}

return jsunity.done();
