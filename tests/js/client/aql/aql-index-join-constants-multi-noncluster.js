/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require('internal');
const _ = require('lodash');

const IndexJoinTestSuite = function () {
  const createCollection = function (name, shardKeys, prototype) {
    return db._create(name);
  };

  const databaseName = "IndexJoinDB";

  const keys = ["x", "y", "z", "w"];

  const testsuite = {
    setUpAll: function () {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);

      for (const name of ["A", "B", "C"]) {
        createCollection(name, keys).ensureIndex({type: "persistent", fields: keys});
        console.warn(db[name].properties());
      }
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },
  };

  const buildQuery = function (a, b, c) {

    let query = `
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FOR doc3 IN C
    `;

    for (let k = 0; k < a; k++) {
      query += `FILTER doc1.${keys[k]} == ${k}\n`;
    }
    for (let k = 0; k < b; k++) {
      query += `FILTER doc2.${keys[k]} == ${k}\n`;
    }
    query += `FILTER doc2.${keys[b]} == doc1.${keys[a]}\n`;
    for (let k = 0; k < c; k++) {
      query += `FILTER doc3.${keys[k]} == ${k}\n`;
    }
    query += `FILTER doc3.${keys[c]} == doc1.${keys[a]}\n`;
    query += "RETURN [doc1, doc2, doc3]";
    return query;
  };

  const testFunction = function (a, b, c) {
    return function () {
      const query = buildQuery(a, b, c);

      const plan = db._createStatement(query).explain().plan;
      const nodes = plan.nodes.map(x => x.type);
      if (nodes.indexOf("JoinNode") === -1) {
        db._explain(query);
      }
      assertEqual(nodes.indexOf("JoinNode"), 1);
      assertEqual(nodes.indexOf("IndexNode"), -1);

    };
  };

  for (let a = 0; a < keys.length; a++) {
    for (let b = 0; b < keys.length; b++) {
      for (let c = 0; c < keys.length; c++) {
        testsuite[`testJoinConstants_${a}${b}${c}`] = testFunction(a, b, c);
      }
    }
  }


  return testsuite;
};

jsunity.run(IndexJoinTestSuite);
return jsunity.done();
