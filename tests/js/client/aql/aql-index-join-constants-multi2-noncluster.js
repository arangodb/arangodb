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
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const internal = require('internal');
const _ = require('lodash');

const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

const IndexJoinTestSuite = function () {
  const createCollection = function (name, shardKeys, prototype) {
    shardKeys = shardKeys || ["x"];
    prototype = prototype || "prototype";
    if (isCluster) {
      if (!db[prototype]) {
        db._create(prototype, {numberOfShards: 3});
      }
      return db._create(name, {numberOfShards: 3, shardKeys: shardKeys, distributeShardsLike: prototype});
    } else {
      return db._create(name);
    }
  };

  const databaseName = "IndexJoinDB";

  const keys = ["x", "y", "z"];

  const computeAllSubset =
    arr => arr.reduce(
      (subsets, value) => subsets.concat(
        subsets.map(set => [...set, value])
      ),
      [[]]
    );

  const computePairs = (a, b) =>
    a.map(x => b.map(y => [x, y])).flat();

  const testsuite = {
    setUpAll: function () {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);

      for (const name of ["A", "B", "C"]) {
        createCollection(name, keys).ensureIndex({type: "persistent", fields: keys});
      }
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },
  };

  const buildQuery = function (doc1Const, doc2Const, doc3Const, doc12Join, doc13Join) {

    let query = `
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FOR doc3 IN C
    `;

    const buildJoin = function (v1, v2, attribs) {
      query += `FILTER ${v1}.${attribs[0]} == ${v2}.${attribs[1]}\n`;
    };

    buildJoin("doc1", "doc2", doc12Join);
    buildJoin("doc1", "doc3", doc13Join);

    let k = 0;
    const buildConstFilter = function (v, attribs) {
      for (const attr of attribs) {
        query += `FILTER ${v}.${attr} == ${k++}\n`;
      }
    };

    buildConstFilter("doc1", doc1Const);
    buildConstFilter("doc2", doc2Const);
    buildConstFilter("doc3", doc3Const);

    query += "RETURN [doc1, doc2, doc3]";
    return query;
  };

  const checkExpectJoin = function (doc1Const, doc2Const, doc3Const, doc12Join, doc13Join) {
    const checkJoinable = function (consts, joinAttrib) {
      if (consts.indexOf(joinAttrib) !== -1) {
        return false;
      }
      const joinIdx = keys.indexOf(joinAttrib);
      if (consts.length < joinIdx) {
        return false;
      }

      let idx = 0;
      for (const c of consts) {
        if (idx > joinIdx) {
          break;
        }
        if (c !== keys[idx]) {
          return false;
        }
        idx += 1;
      }

      return true;
    };

    let count = 0;

    if (checkJoinable(doc1Const, doc12Join[0]) && checkJoinable(doc2Const, doc12Join[1])) {
      count++;
    }

    if (checkJoinable(doc1Const, doc13Join[0]) &&checkJoinable(doc3Const, doc13Join[1])) {
      count++;
    }

    if (count === 2) {
      if (doc12Join[0] !== doc13Join[0]) {
        return 1;
      }
    }
    if (count === 0) {
      return false;
    }
    return count;
  };

  const opts = {
    optimizer: {rules: ["-propagate-constant-attributes"]}
  };

  const testFunction = function (query, expectJoinFN) {
    return function () {
      const expectJoin = expectJoinFN();
      const plan = db._createStatement(query, null, opts).explain().plan;
      const nodes = plan.nodes.map(x => x.type);
      
      if (expectJoin !== false) {
        if (nodes.indexOf("JoinNode") === -1) {
          db._explain(query, null, opts);
        }
        assertEqual(nodes.indexOf("JoinNode"), 1);

      } else {
        if (nodes.indexOf("JoinNode") !== -1) {
          db._explain(query, null, opts);
        }
        assertEqual(nodes.indexOf("JoinNode"), -1);
      }
    };
  };

  const subsets = computeAllSubset(keys);
  const pairs = computePairs(keys, keys);

  for (const a of subsets) {
    for (const b of subsets) {
      for (const c of subsets) {
        for (const d of pairs) {
          for (const e of pairs) {
            testsuite[`testJoinConstants_${a}_${b}_${c}_${d}_${e}`] =
              testFunction(buildQuery(a, b, c, d, e), () => checkExpectJoin(a, b, c, d, e));
          }
        }
      }
    }
  }

  return testsuite;
};

jsunity.run(IndexJoinTestSuite);
return jsunity.done();
