/* global fail */
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
/// @author Lars Maier
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const aql = arangodb.aql;
const ERRORS = arangodb.errors;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");

const database = "MdiUniqueClusterTestDatabase";

function mdiClusterUniqueShardKeys() {

  const checkUnsupported = function (c, index) {
    try {
      c.ensureIndex(index);
      fail();
    } catch (e) {
      assertEqual(ERRORS.ERROR_CLUSTER_UNSUPPORTED.code, e.errorNum);
    }
  };

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    setUp: function () {
      db._create("c", {numberOfShards: 3, shardKeys: ["x"]});
    },
    tearDown: function () { db.c.drop(); },

    testMdiClusterUnique: function () {
      // This should work as expected
      db.c.ensureIndex({type: 'mdi', fields: ["x", "y"], fieldValueTypes: 'double', unique: true});
      db.c.ensureIndex({type: 'mdi', fields: ["y", "x", "z"], fieldValueTypes: 'double', unique: true});

      // This should not work
      checkUnsupported(db.c, {type: 'mdi', fields: ["z", "y"], fieldValueTypes: 'double', unique: true});
      checkUnsupported(db.c, {type: 'mdi', fields: ["z"], fieldValueTypes: 'double', unique: true});
      checkUnsupported(db.c, {type: 'mdi', fields: ["y"], fieldValueTypes: 'double', unique: true});
    },

    testMdiPrefixClusterUnique: function () {
      // This should work as expected
      db.c.ensureIndex({
        type: 'mdi-prefixed',
        fields: ["z", "y"],
        prefixFields: ["x"],
        fieldValueTypes: 'double',
        unique: true
      });
      db.c.ensureIndex({
        type: 'mdi-prefixed',
        fields: ["x", "y"],
        prefixFields: ["z"],
        fieldValueTypes: 'double',
        unique: true
      });

      // This should not work
      checkUnsupported(db.c, {
        type: 'mdi-prefixed',
        fields: ["z", "y"],
        prefixFields: ["a"],
        fieldValueTypes: 'double',
        unique: true
      });
      checkUnsupported(db.c, {
        type: 'mdi-prefixed',
        fields: ["z"],
        prefixFields: ["a"],
        fieldValueTypes: 'double',
        unique: true
      });
    }

  };
}

jsunity.run(mdiClusterUniqueShardKeys);

return jsunity.done();
