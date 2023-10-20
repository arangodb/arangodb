/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, AQL_EXECUTE, AQL_EXPLAIN */

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

const indexJoinTestMultiSuite = function (parameters) {

  const indexAttribute = "x";
  const filterAttribute = "y";
  const projectionAttribute = "z";
  if (isCluster && !isEnterprise) {
    return {};
  }

  const createCollection = function (name) {
    if (isCluster) {
      if (!db.prototype) {
        db._create("prototype", {numberOfShards: 3});
      }
      return db._create(name, {numberOfShards: 3, shardKeys: [indexAttribute], distributeShardsLike: "prototype"});
    } else {
      return db._create(name);
    }
  };

  const databaseName = "IndexJoinDB";
  const collection1 = "A";
  const collection2 = "B";

  const numDocuments = 2000;
  const documentsA = Array.from({length: numDocuments}).map(function (v, idx) { return {x: idx, y: 2 * idx, z: 3 * idx + 1}; });
  const documentsB = Array.from({length: numDocuments}).map(function (v, idx) { return {x: idx, y: 2 * idx, z: 3 * idx}; });

  const buildQuery = function(config) {
    return `
  FOR d1 IN ${collection1}
    SORT d1.${indexAttribute} 
    FOR d2 IN ${collection2}
      FILTER d1.${indexAttribute} == d2.${indexAttribute}
      FILTER ${config.filter1 ? `d1.${filterAttribute} % 4 == 0` : "TRUE"}
      FILTER ${config.filter2 ? `d2.${filterAttribute} % 4 == 0` : "TRUE"}
      RETURN [
        ${config.projection1 ? `{z: d1.${projectionAttribute}}` : 'KEEP(d1, "x", "y", "z")'},
        ${config.projection2 ? `{z: d2.${projectionAttribute}}` : 'KEEP(d2, "x", "y", "z")'},
      ]
    `;
  };

  const buildIndex = function (collection, projected, filtered, unique) {

    const fields = ["x"];
    const storedValues = [];

    const addField = function(fieldName, where) {
      if (where === "indexed") {
        fields.push(fieldName);
      } else if (where === "stored") {
        storedValues.push(fieldName);
      }
    };

    addField(projectionAttribute, projected);
    addField(filterAttribute, filtered);
    collection.ensureIndex({
      type: "persistent",
      fields: fields,
      name: collection.name() + "_idx",
      storedValues: storedValues.length > 0 ? storedValues : undefined,
      unique: unique,
    });
  };

  const buildCollections = function (config) {
    const A = createCollection(collection1);
    buildIndex(A, config.projectedAttribute1, config.filterAttribute1, config.unique);
    A.save(documentsA);
    const B = createCollection(collection2);
    buildIndex(B, config.projectedAttribute2, config.filterAttribute2, config.unique);
    B.save(documentsB);
  };

  const computeResult = function (config) {
    return _.zip(documentsA, documentsB).filter(function ([docA, docB]) {
      return (!config.filter1 || docA[filterAttribute] % 4 === 0) &&
          (!config.filter2 || docB[filterAttribute] % 4 === 0);
    }).map(function ([docA, docB]) {
      return [config.projection1 ? {z: docA.z} : docA, config.projection2 ? {z: docB.z} : docB];
    });
  };

  const checkIndexInfos = function (infos, filter, filterAttr, projection, projectionAttr) {
    if (filter) {
      assertNotEqual(infos.filter, undefined);
      assertEqual(normalize(infos.filterProjections), normalize(["y"]));
      assertEqual(infos.indexCoversFilterProjections, ["indexed", "stored"].indexOf(filterAttr) !== -1);
    } else {
      assertEqual(infos.filter, undefined);
    }

    if (projection) {
      const proj = _.flatten(normalize(infos.projections));
      assertNotEqual(proj.indexOf("z"), -1);
      if (!filter || infos.indexCoversFilterProjections) {
        assertEqual(infos.indexCoversProjections, ["indexed", "stored"].indexOf(projectionAttr) !== -1, infos);
      } else {
        assertEqual(infos.indexCoversProjections, false);
      }
    } else {
      assertEqual(normalize(infos.projections), normalize([]));
    }

    if (filter && !infos.indexCoversFilterProjections) {
      assertTrue(!infos.indexCoversProjections);
    }
  };

  const getIndexForCollection = function(indexInfos, collectionName) {
    const indexName = collectionName + "_idx";
    for (const info of indexInfos) {
      if (info.index.name === indexName) {
        return info;
      }
    }
    assertTrue(false, "index not found");
  };

  const generateTestFunction = function (config) {
    return function () {
      const query = buildQuery(config);

      const plan = db._createStatement({query: query}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);
      assertEqual(nodes.indexOf("JoinNode"), 1);

      const join = plan.nodes[1];

      // it can happen that the indexes are swap, so we have to find the correct index for collection A

      checkIndexInfos(getIndexForCollection(join.indexInfos, collection1), config.filter1, config.filterAttribute1,
          config.projection1, config.projectedAttribute1);
      checkIndexInfos(getIndexForCollection(join.indexInfos, collection2), config.filter2, config.filterAttribute2,
          config.projection2, config.projectedAttribute2);

      const expectedResult = computeResult(config);
      // sorting by x is required, because the query does not imply any sorting order
      const actualResult = db._query(query).toArray().sort((left, right) => left[0].z - right[0].z);

      // db._explain(query);
      // print(expectedResult);
      // print(actualResult);

      assertEqual(expectedResult.length, actualResult.length);
      for (const [expected, actual] of _.zip(expectedResult, actualResult)) {
        assertEqual(expected, actual);
      }
    };
  };

  let testsuite = {
    setUpAll: function () {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);
      buildCollections(parameters[0]);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },
  };

  const configToString = function(config) {
    let str = "";
    if (config.projection1) { str += "p1"; }
    if (config.projection2) { str += "p2"; }
    if (config.filter1) { str += "f1"; }
    if (config.filter2) { str += "f2"; }
    return str;
  };
  const configDDLToString = function (config) {
    return (config.unique ? "U" : "") + config.projectedAttribute1[0] + config.projectedAttribute2[0] +
        config.filterAttribute1[0] + config.filterAttribute2[0];
  };

  for (const config of parameters) {
    testsuite["testJoin_" + configDDLToString(config) + "_" + configToString(config)] = generateTestFunction(config);
  }

  return testsuite;
};

const generateParameters = function(prototype) {
  const entries = Object.entries(prototype);

  let list = [];
  const recurse = function (result, entries, idx) {
    if (entries.length === idx) {
      list.push(Object.assign({}, result));
    } else {
      const [key, values] = entries[idx];
      for (const v of values){
        result[key] = v;
        recurse(result, entries, idx + 1);
      }
    }
  };

  recurse({}, entries, 0);
  return list;
};

// We group the tests by the same data definition and then run all possible queries at once.
const parameters = _.groupBy(generateParameters({
  projection1: [true, false],
  projection2: [true, false],
  filter1: [true, false],
  filter2: [true, false],
  filterAttribute1: ["indexed", "stored", "document"],
  filterAttribute2: ["indexed", "stored", "document"],
  projectedAttribute1: ["indexed", "stored", "document"],
  projectedAttribute2: ["indexed", "stored", "document"],
  unique: [true, false],
}), (c) => ["filterAttribute1", "filterAttribute2", "projectedAttribute1", "projectedAttribute2", "unique"].map(k => c[k]).join(""));

for (const configs of Object.values(parameters)) {
  jsunity.run(function () {
    return indexJoinTestMultiSuite(configs);
  });
}
return jsunity.done();
