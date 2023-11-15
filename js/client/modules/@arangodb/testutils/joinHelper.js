/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

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

const db = require("@arangodb").db;
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const internal = require('internal');
const _ = require('lodash');

const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

const numDocuments = 2000;
const indexAttribute = "x";
const filterAttribute = "y";
const projectionAttribute = "z";

const databaseName = "IndexJoinDB";
const collection1 = "A";
const collection2 = "B";

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

const queryOptions = {
  optimizer: {
    rules: ["-late-document-materialization"], // turn off late materialization
  },
  maxNumberOfPlans: 1
};

const documentsA = Array.from({length: numDocuments})
    .map(function (v, idx) {
      return {x: idx, y: 2 * idx, z: 3 * idx + 1};
    });
const documentsB = Array.from({length: numDocuments})
    .map(function (v, idx) {
      return {x: idx, y: 2 * idx, z: 3 * idx};
    });

const buildQuery = function (config) {
  const usageToString = function (varName, usage) {
    if (usage === "projection") {
      return `{z: ${varName}.${projectionAttribute}}`;
    } else if (usage === "document") {
      return `KEEP(${varName}, "x", "y", "z")`;
    } else {
      return 'NULL';
    }
  };

  return `
  FOR d1 IN ${collection1}
    SORT d1.${indexAttribute} 
    FOR d2 IN ${collection2}
      FILTER d1.${indexAttribute} == d2.${indexAttribute}
      FILTER ${config.filter1 ? `d1.${filterAttribute} % 4 == 0` : "TRUE"}
      FILTER ${config.filter2 ? `d2.${filterAttribute} % 4 == 0` : "TRUE"}
      ${config.sort ? "SORT d1.z" : "/* no sort */"}
      ${config.limit === false ? "/* no limit */" : `LIMIT ${config.limit[0]}, ${config.limit[1]}`}
      RETURN [
        ${usageToString('d1', config.valueUsage1)},
        ${usageToString('d2', config.valueUsage2)},
      ]
    `;
};

const buildIndex = function (collection, projected, filtered, unique) {
  const fields = ["x"];
  const storedValues = [];

  const addField = function (fieldName, where) {
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
  const result = _.zip(documentsA, documentsB).filter(function ([docA, docB]) {
    return (!config.filter1 || docA[filterAttribute] % 4 === 0) &&
        (!config.filter2 || docB[filterAttribute] % 4 === 0);
  }).map(function ([docA, docB]) {

    const handleDoc = function (doc, usage) {
      if (usage === "projection") {
        return {z: doc.z};
      } else if (usage === "document") {
        return doc;
      } else {
        return null;
      }
    };

    return [handleDoc(docA, config.valueUsage1), handleDoc(docB, config.valueUsage2)];
  });

  if (config.limit === false) {
    return result;
  } else {
    return _.take(_.drop(result, config.limit[0]), config.limit[1]);
  }
};

const checkIndexInfos = function (infos, filter, filterAttr, valueUsage, projectionAttr) {
  if (filter) {
    assertNotEqual(infos.filter, undefined);
    assertEqual(normalize(infos.filterProjections), normalize(["y"]));
    assertEqual(infos.indexCoversFilterProjections, ["indexed", "stored"].indexOf(filterAttr) !== -1);
  } else {
    assertEqual(infos.filter, undefined);
  }

  const documentUsedForFilter = filter && !infos.indexCoversFilterProjections;

  if (valueUsage === "projection") {
    assertEqual(infos.producesOutput, true);
    const proj = _.flatten(normalize(infos.projections));
    assertEqual(proj, ["z"], proj);
    if (!filter || infos.indexCoversFilterProjections) {
      assertEqual(infos.indexCoversProjections, ["indexed", "stored"].indexOf(projectionAttr) !== -1, infos);
    } else {
      assertEqual(infos.indexCoversProjections, false);
    }
  } else if (valueUsage === "document") {
    assertEqual(infos.producesOutput, true);
    assertEqual(normalize(infos.projections), normalize([]));
  } else {
    assertEqual(infos.producesOutput, documentUsedForFilter, infos);
  }

  if (filter && !infos.indexCoversFilterProjections) {
    assertTrue(!infos.indexCoversProjections);
  }
};

const getIndexForCollection = function (indexInfos, collectionName) {
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

    const plan = db._createStatement({query, options: queryOptions}).explain().plan;
    const nodes = plan.nodes.map(x => x.type);
    if (nodes.indexOf("JoinNode") === -1) {
      db._explain(query, null, queryOptions);
    }
    assertEqual(nodes.indexOf("JoinNode"), 1);

    const join = plan.nodes[1];

    // it can happen that the indexes are swap, so we have to find the correct index for collection A
    checkIndexInfos(getIndexForCollection(join.indexInfos, collection1), config.filter1, config.filterAttribute1,
        config.valueUsage1, config.projectedAttribute1);
    checkIndexInfos(getIndexForCollection(join.indexInfos, collection2), config.filter2, config.filterAttribute2,
        config.valueUsage2, config.projectedAttribute2);

    const expectedResult = computeResult(config);
    // sorting by x is required, because the query does not imply any sorting order
    const actualResult = db._query(query, null, queryOptions)
        .toArray().sort((left, right) => left[0].z - right[0].z);
    // db._explain(query);
    // print(expectedResult);
    // print(actualResult);

    assertEqual(expectedResult.length, actualResult.length);
    if (config.sort || !isCluster) {
      // On single server or with explicit sort we can assort a correct sert order in combination with limit
      // otherwise the result is undefined.
      for (const [expected, actual] of _.zip(expectedResult, actualResult)) {
        assertEqual(expected, actual);
      }
    } else {
      // The result is actually undefined, we can only check for internal correctness, i.e. the entries actually
      // match the join condition
      for (const [docA, docB] of actualResult) {
        if (config.valueUsage2 === "null") {
          assertEqual(docB, null);
          assertNotEqual(docA, null);
          assertNotEqual(docA.z, undefined);
        } else {
          assertEqual(docA.z, docB.z + 1);
        }
      }
    }
  };
};

const generateParameters = function (prototype) {
  const entries = Object.entries(prototype);

  let list = [];
  const recurse = function (result, entries, idx) {
    if (entries.length === idx) {
      list.push(Object.assign({}, result));
    } else {
      const [key, values] = entries[idx];
      for (const v of values) {
        result[key] = v;
        recurse(result, entries, idx + 1);
      }
    }
  };

  recurse({}, entries, 0);
  return list;
};

const indexJoinTestMultiSuite = function (parameters) {
  if (isCluster && !isEnterprise) {
    return {};
  }

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

  const configToString = function (config) {
    let str = "";
    if (config.valueUsage1 === "projection") {
      str += "p1";
    }
    if (config.valueUsage1 === "document") {
      str += "d1";
    }
    if (config.valueUsage2 === "projection") {
      str += "p2";
    }
    if (config.valueUsage2 === "document") {
      str += "d2";
    }
    if (config.filter1) {
      str += "f1";
    }
    if (config.filter2) {
      str += "f2";
    }
    if (config.sort) {
      str += "S";
    }
    if (config.limit !== false) {
      str += `L_${config.limit[0]}_${config.limit[1]}`;
    }
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

const createIndexJoinTestMultiSuite = function (configs) {
  return indexJoinTestMultiSuite(configs);
};

const createParameters = function (unique, sort, bucket, buckets) {
  // We group the tests by the same data definition and then run all possible queries at once.
  let tests = _.groupBy(generateParameters({
    valueUsage1: ["projection", "document"],
    valueUsage2: ["projection", "document", "null"],
    filter1: [true, false],
    filter2: [true, false],
    filterAttribute1: ["indexed", "stored", "document"],
    filterAttribute2: ["indexed", "stored", "document"],
    projectedAttribute1: ["indexed", "stored", "document"],
    projectedAttribute2: ["indexed", "stored", "document"],
    sort: [sort],
    limit: [
      false,
      [0, numDocuments / 20],
      [0, numDocuments / 2],
      [numDocuments / 4, numDocuments / 2],
      [numDocuments / 2, numDocuments],
    ],
    unique: [unique],
  }), (c) => ["filterAttribute1", "filterAttribute2", "projectedAttribute1", "projectedAttribute2", "unique"].map(k => c[k]).join(""));

  if (bucket >= buckets || buckets < 1) {
    throw "invalid number of buckets: " + bucket + " / " + buckets;
  }

  if (buckets !== 1) {
    // partition tests into buckets
    let keys = Object.keys(tests);
    const lower = Math.floor(bucket * (keys.length / buckets));
    const upper = Math.floor((bucket + 1) * (keys.length / buckets));
    keys = keys.slice(lower, upper);
    let bucketized = {};
    keys.forEach((k) => {
      bucketized[k] = tests[k];
    });
    tests = bucketized;
  }

  return tests;
};

exports.createIndexJoinTestMultiSuite = createIndexJoinTestMultiSuite;
exports.createParameters = createParameters;
