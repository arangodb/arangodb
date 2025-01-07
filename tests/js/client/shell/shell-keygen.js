/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertTrue, assertFalse,, assertMatch, fail */

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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;
const {getDBServers} = require("@arangodb/test-helper");
const cluster = require("internal").isCluster();
const isEnterprise = require("internal").isEnterprise();
const internal = require("internal");
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

let graphs = require("@arangodb/general-graph");
if (isEnterprise) {
  graphs = require("@arangodb/smart-graph");
}

const gn = "UnitTestsGraph";
const vn = "UnitTestsVertex";
const en = "UnitTestsEdge";
const cn = "UnitTestsKeyGen";

// in single server case, this is the single server.
// in cluster case, this is the coordinator.
const filter = cluster ? instanceRole.dbServer: instanceRole.single;

const disableSingleDocRule = {optimizer: {rules: ["-optimize-cluster-single-document-operations"]}};
const disableRestrictToSingleShardRule = {optimizer: {rules: ["-restrict-to-single-shard"]}};

const runSmartVertexInserts = (graphProperties) => {
  graphs._create(gn, [graphs._relation(en, vn, vn)], null, graphProperties);

  try {
    // test various ways of inserting documents into the collection

    // single insert, using document API
    db[vn].insert({value: "42"});

    // batch insert, using document API
    db[vn].insert([{value: "42"}, {value: "42"}, {value: "42"}]).forEach((res) => {
      assertFalse(res.hasOwnProperty('error'));
    });

    // single insert, using AQL
    db._query(`INSERT
          { value: "42" } INTO
          ${vn}`);

    db._query(`INSERT
          { value: "42" } INTO
          ${vn}`, null, disableSingleDocRule);

    db._query(`INSERT
          { value: "42" } INTO
          ${vn}`, null, disableRestrictToSingleShardRule);

    // batch insert, using AQL
    db._query(`FOR i IN 1..3 INSERT { value: "42" } INTO ${vn}`);

    db._query(`FOR i IN 1..3 INSERT { value: "42" } INTO ${vn}`, null, disableRestrictToSingleShardRule);

    // 13 documents inserted so far. Try to insert some malformed ones (must fail).
    insertInvalidSmartVertices(vn);

    assertEqual(13, db[vn].count());
  } finally {
    graphs._drop(gn, true);
  }
};

const runSmartEdgeInserts = (graphProperties) => {
  // Helper method to either insert edges into SmartGraphs or EnterpriseGraphs
  if (cluster) {
    // fail if we generate a key on a DB server
    IM.debugSetFailAt("KeyGenerator::generateOnSingleServer", filter);
  } else {
    // single server: we can actually get here with the SmartGraph simulator!
    IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);
  }

  graphs._create(gn, [graphs._relation(en, vn, vn)], null, graphProperties);

  try {
    // test various ways of inserting documents into the collection

    // single insert, using document API
    db[en].insert({value: "42", _from: vn + "/test:42", _to: vn + "/test:42"});

    // batch insert, using document API
    db[en].insert([{value: "42", _from: vn + "/test:42", _to: vn + "/test:42"}, {
      value: "42",
      _from: vn + "/test:42",
      _to: vn + "/test:42"
    }, {value: "42", _from: vn + "/test:42", _to: vn + "/test:42"}]).forEach((res) => {
      assertFalse(res.hasOwnProperty('error'));
    });

    // single insert, using AQL
    db._query(`INSERT
          { value: "42", _from: "${vn}/test:42", _to: "${vn}/test:42" } INTO
          ${en}`);
    db._query(`INSERT
          { value: "42", _from: "${vn}/test:42", _to: "${vn}/test:42" } INTO
          ${en}`, null, disableSingleDocRule);
    db._query(`INSERT
          { value: "42", _from: "${vn}/test:42", _to: "${vn}/test:42" } INTO
          ${en}`, null, disableRestrictToSingleShardRule);

    // batch insert, using AQL
    db._query(`FOR i IN 1..3 INSERT { value: "42", _from: "${vn}/test:42", _to: "${vn}/test:42" } INTO ${en}`);
    db._query(`FOR i IN 1..3 INSERT { value: "42", _from: "${vn}/test:42", _to: "${vn}/test:42" } INTO ${en}`, null, disableRestrictToSingleShardRule);

    // 13 documents inserted so far. Try to insert some malformed ones (must fail).
    insertInvalidSmartEdges(en);

    assertEqual(13, db[en].count());
  } finally {
    graphs._drop(gn, true);
  }
};

const insertInvalidSmartVertices = (vn) => {
  // after all positive tests, now some negative ones (not allowed inserts)
  // we will not add any documents additionally!

  const invalidKeys = [
    "whitespace inbetween", // whitespace not allowed
    12345, // numeric value not allowed
    NaN,  // invalid numeric value not allowed
    null // null not allowed
  ];

  // Note: Using save() <-> insert() should be the same.
  // Using both here to double-check that the assumption is
  // proven.

  // single insert, using document API
  invalidKeys.forEach(invalidKey => {
    try {
      db[vn].insert({_key: invalidKey, value: "43"});
      fail();
    } catch (ignore) {
    }
  });
  // single insert, using document API
  invalidKeys.forEach(invalidKey => {
    try {
      db[vn].save({_key: invalidKey, value: "43"});
      fail();
    } catch (ignore) {
    }
  });

  // batch insert, using document API
  const batch = [];
  invalidKeys.forEach(invalidKey => {
    batch.push({_key: invalidKey, value: "iAmSoSmart"});
  });

  try {
    // batch insert of malformed documents (wrong keys)
    db[vn].insert(batch);
    fail();
  } catch (ignore) {
  }
  try {
    // batch insert of malformed documents (wrong keys)
    db[vn].save(batch);
    fail();
  } catch (ignore) {
  }
};

const insertInvalidSmartEdges = (en) => {
  // after all positive tests, now some negative ones (not allowed inserts)
  // we will not add any documents additionally!

  const invalidKeys = [
    "whitespace inbetween", // whitespace not allowed
    12345, // numeric value not allowed
    NaN,  // invalid numeric value not allowed
    null, // null not allowed
    undefined, // undefined not allowed,
    true, // boolean not allowed
    false // boolean not allowed
  ];

  // single insert, using document API
  invalidKeys.forEach(invalidKey => {
    try {
      const body = {
        _key: invalidKey, value: "43", _from: "dontCare/" + invalidKey, _to: "dontCare/" + invalidKey
      };
      db[en].insert(body);
    } catch (ignore) {
    }
  });

  // batch insert, using document API
  let batch = [];

  invalidKeys.forEach(invalidKey => {
    batch.push({
      _key: invalidKey, _from: "dontCare/" + invalidKey, _to: "dontCare/" + invalidKey
    });
  });

  try {
    // batch insert of malformed documents (wrong keys)
    db[en].save(batch);
  } catch (ignore) {
  }
};

function KeyGeneratorsSuite() {
  'use strict';

  return {
    testAvailableKeyGenerators: function () {
      let result = arango.GET("/_api/key-generators");

      assertTrue(result.hasOwnProperty("keyGenerators"), result);
      let gens = result.keyGenerators;
      assertTrue(Array.isArray(gens), result);
      assertTrue(gens.includes("uuid"), result);
      assertTrue(gens.includes("traditional"), result);
      assertTrue(gens.includes("padded"), result);
      assertTrue(gens.includes("autoincrement"), result);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: traditional key gen
////////////////////////////////////////////////////////////////////////////////

function TraditionalSuite() {
  'use strict';

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKeyNonDefaultSharding1: function () {
      let c = db._create(cn, {shardKeys: ["value"], keyOptions: {type: "traditional", allowUserKeys: false}});

      try {
        c.save({_key: "1234"}); // no user keys allowed
        fail();
      } catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
          err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);
      }
    },

    testCreateInvalidKeyNonDefaultSharding2: function () {
      if (!cluster) {
        return;
      }

      let c = db._create(cn, {shardKeys: ["value"], keyOptions: {type: "traditional", allowUserKeys: true}});

      try {
        c.save({_key: "1234"}); // no user keys allowed
        fail();
      } catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
          err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);
      }
    },

    testCreateKeyNonDefaultSharding: function () {
      if (!cluster) {
        return;
      }

      let c = db._create(cn, {shardKeys: ["value"], keyOptions: {type: "traditional", allowUserKeys: true}});

      let key = c.save({value: "1"}); // no user keys allowed
      let doc = c.document(key);
      assertEqual("1", doc.value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateTraditionalInvalidKey1: function () {
      let c = db._create(cn, {keyOptions: {type: "traditional", allowUserKeys: false}});

      try {
        c.save({_key: "1234"}); // no user keys allowed
        fail();
      } catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
          err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateTraditionalInvalidKey2: function () {
      let c = db._create(cn, {keyOptions: {type: "traditional", allowUserKeys: true}});

      try {
        c.save({_key: "öä .mds 3 -6"}); // invalid key
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateTraditionalOk1: function () {
      let c = db._create(cn, {keyOptions: {type: "traditional"}});

      let options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateTraditionalOk2: function () {
      let c = db._create(cn, {keyOptions: {}});

      let options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateTraditionalOk3: function () {
      let c = db._create(cn, {keyOptions: {allowUserKeys: false}});

      let options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(false, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with user key
////////////////////////////////////////////////////////////////////////////////

    testCheckTraditionalUserKey: function () {
      let c = db._create(cn, {keyOptions: {type: "traditional", allowUserKeys: true}});

      let options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);

      let d1 = c.save({_key: "1234"});
      assertEqual("1234", d1._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValues: function () {
      let c = db._create(cn, {keyOptions: {type: "traditional"}});

      let d1 = parseFloat(c.save({})._key);
      let d2 = parseFloat(c.save({})._key);
      let d3 = parseFloat(c.save({})._key);
      let d4 = parseFloat(c.save({})._key);
      let d5 = parseFloat(c.save({})._key);

      assertTrue(d1 < d2);
      assertTrue(d2 < d3);
      assertTrue(d3 < d4);
      assertTrue(d4 < d5);
    },

    testInvalidKeyGenerator: function () {
      try {
        db._create(cn, {keyOptions: {type: "der-fuchs"}});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_INVALID_KEY_GENERATOR.code, err.errorNum);
      }
    },

    testAutoincrementGeneratorInCluster: function () {
      if (!cluster) {
        return;
      }

      try {
        db._create(cn, {keyOptions: {type: "autoincrement"}, numberOfShards: 2});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_UNSUPPORTED.code, err.errorNum);
      }
    },

    testUuid: function () {
      let c = db._create(cn, {keyOptions: {type: "uuid"}});

      let options = c.properties().keyOptions;
      assertEqual("uuid", options.type);
      assertTrue(options.allowUserKeys);

      for (let i = 0; i < 100; ++i) {
        let doc = c.insert({});
        assertMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/, doc._key);
      }
    },

    testPadded: function () {
      let c = db._create(cn, {keyOptions: {type: "padded"}});

      let options = c.properties().keyOptions;
      assertEqual("padded", options.type);
      assertTrue(options.allowUserKeys);

      for (let i = 0; i < 100; ++i) {
        let doc = c.insert({});
        assertEqual(16, doc._key.length);
        assertMatch(/^[0-9a-f]{16}$/, doc._key);
      }
    },

    testTraditionalTracking: function () {
      let c = db._create(cn, {keyOptions: {type: "traditional"}});

      let options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertTrue(options.allowUserKeys);

      let lastKey = null;
      for (let i = 0; i < 100; ++i) {
        let key = c.insert({})._key;
        if (lastKey !== null) {
          assertTrue(Number(key) > Number(lastKey));
        }
        lastKey = key;
      }

      if (!cluster) {
        assertEqual(lastKey, c.properties().keyOptions.lastValue);
      }
    },

    testPaddedTracking: function () {
      let c = db._create(cn, {keyOptions: {type: "padded"}});

      let options = c.properties().keyOptions;
      assertEqual("padded", options.type);
      assertTrue(options.allowUserKeys);

      let lastKey = null;
      for (let i = 0; i < 100; ++i) {
        let key = c.insert({})._key;
        assertEqual(16, key.length);
        if (lastKey !== null) {
          assertTrue(key > lastKey);
        }
        lastKey = key;
      }

      if (!cluster) {
        let hex = c.properties().keyOptions.lastValue.toString(16);
        assertEqual(lastKey, Array(16 + 1 - hex.length).join("0") + hex);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: auto-increment
////////////////////////////////////////////////////////////////////////////////

function AutoIncrementSuite() {
  'use strict';

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid offset
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidOffset: function () {
      try {
        db._create(cn, {keyOptions: {type: "autoincrement", offset: -1}});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_INVALID_KEY_GENERATOR.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement1: function () {
      try {
        db._create(cn, {keyOptions: {type: "autoincrement", increment: 0}});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_INVALID_KEY_GENERATOR.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement2: function () {
      try {
        db._create(cn, {keyOptions: {type: "autoincrement", increment: -1}});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_INVALID_KEY_GENERATOR.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with invalid increment
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidIncrement3: function () {
      try {
        db._create(cn, {keyOptions: {type: "autoincrement", increment: 9999999999999}});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_INVALID_KEY_GENERATOR.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateAutoIncrementInvalidKey1: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", allowUserKeys: false}});

      try {
        c.save({_key: "1234"}); // no user keys allowed
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateAutoIncrementInvalidKey2: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", allowUserKeys: true}});

      try {
        c.save({_key: "a1234"}); // invalid key
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateAutoincrementOk1: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", offset: 12345678901234567}});

      let options = c.properties().keyOptions;
      assertEqual("autoincrement", options.type);
      assertEqual(true, options.allowUserKeys);
      assertEqual(1, options.increment);
      assertEqual(12345678901234567, options.offset);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateAutoincrementOk2: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement"}});

      let options = c.properties().keyOptions;
      assertEqual("autoincrement", options.type);
      assertEqual(true, options.allowUserKeys);
      assertEqual(1, options.increment);
      assertEqual(0, options.offset);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateAutoIncrementOk3: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", allowUserKeys: false, offset: 83, increment: 156}});

      let options = c.properties().keyOptions;
      assertEqual("autoincrement", options.type);
      assertEqual(false, options.allowUserKeys);
      assertEqual(156, options.increment);
      assertEqual(83, options.offset);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with null keygen
////////////////////////////////////////////////////////////////////////////////

    testCreateNullKeyGen: function() {
      // NOTE: This is only passing for 3.11 compatibility mode
      let c = db._create(cn, {keyOptions: null});

      let options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with user key
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoIncrementUserKey: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", allowUserKeys: true}});

      let d1 = c.save({_key: "1234"});
      assertEqual("1234", d1._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValues1: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", offset: 2, increment: 3}});

      let d1 = c.save({});
      assertEqual("2", d1._key);

      let d2 = c.save({});
      assertEqual("5", d2._key);

      let d3 = c.save({});
      assertEqual("8", d3._key);

      let d4 = c.save({});
      assertEqual("11", d4._key);

      let d5 = c.save({});
      assertEqual("14", d5._key);

      // create a gap
      let d6 = c.save({_key: "100"});
      assertEqual("100", d6._key);

      let d7 = c.save({});
      assertEqual("101", d7._key);

      let d8 = c.save({});
      assertEqual("104", d8._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValues2: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", offset: 19, increment: 1}});

      let d1 = c.save({});
      assertEqual("19", d1._key);

      let d2 = c.save({});
      assertEqual("20", d2._key);

      let d3 = c.save({});
      assertEqual("21", d3._key);

      let d4 = c.save({});
      assertEqual("22", d4._key);

      let d5 = c.save({});
      assertEqual("23", d5._key);

      // create a gap
      let d6 = c.save({_key: "99"});
      assertEqual("99", d6._key);

      let d7 = c.save({});
      assertEqual("100", d7._key);

      let d8 = c.save({});
      assertEqual("101", d8._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values sequence
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesSequence1: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", offset: 1, increment: 4}});

      let d1 = c.save({});
      assertEqual("1", d1._key);

      let d2 = c.save({});
      assertEqual("5", d2._key);

      let d3 = c.save({});
      assertEqual("9", d3._key);

      let d4 = c.save({});
      assertEqual("13", d4._key);

      d1 = c.save({});
      assertEqual("17", d1._key);

      d2 = c.save({});
      assertEqual("21", d2._key);

      d3 = c.save({});
      assertEqual("25", d3._key);

      d4 = c.save({});
      assertEqual("29", d4._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values sequence
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesSequence2: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", offset: 0, increment: 2}});

      let d1 = c.save({});
      assertEqual("2", d1._key);

      let d2 = c.save({});
      assertEqual("4", d2._key);

      let d3 = c.save({});
      assertEqual("6", d3._key);

      let d4 = c.save({});
      assertEqual("8", d4._key);

      d1 = c.save({});
      assertEqual("10", d1._key);

      // create a gap
      d2 = c.save({_key: "39"});
      assertEqual("39", d2._key);

      d3 = c.save({});
      assertEqual("40", d3._key);

      // create a gap
      d4 = c.save({_key: "19567"});
      assertEqual("19567", d4._key);

      d1 = c.save({});
      assertEqual("19568", d1._key);

      d2 = c.save({});
      assertEqual("19570", d2._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesMixedWithUserKeys: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", offset: 0, increment: 1}});

      let d1 = c.save({});
      assertEqual("1", d1._key);

      let d2 = c.save({});
      assertEqual("2", d2._key);

      let d3 = c.save({_key: "100"});
      assertEqual("100", d3._key);

      let d4 = c.save({_key: "3"});
      assertEqual("3", d4._key);

      let d5 = c.save({_key: "99"});
      assertEqual("99", d5._key);

      let d6 = c.save({});
      assertEqual("101", d6._key);

      let d7 = c.save({});
      assertEqual("102", d7._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValuesDuplicates: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", offset: 0, increment: 1}});

      let d1 = c.save({});
      assertEqual("1", d1._key);

      let d2 = c.save({});
      assertEqual("2", d2._key);

      try {
        c.save({_key: "1"});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test out of keys
////////////////////////////////////////////////////////////////////////////////

    testOutOfKeys1: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", allowUserKeys: true, offset: 0, increment: 1}});

      let d1 = c.save({_key: "18446744073709551615"}); // still valid
      assertEqual("18446744073709551615", d1._key);

      try {
        c.save({}); // should raise out of keys
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_OUT_OF_KEYS.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test out of keys
////////////////////////////////////////////////////////////////////////////////

    testOutOfKeys2: function () {
      let c = db._create(cn, {keyOptions: {type: "autoincrement", allowUserKeys: true, offset: 0, increment: 10}});

      let d1 = c.save({_key: "18446744073709551615"}); // still valid
      assertEqual("18446744073709551615", d1._key);

      try {
        c.save({}); // should raise out of keys
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_OUT_OF_KEYS.code, err.errorNum);
      }
    }

  };
}

function AllowUserKeysSuite() {
  'use strict';

  let generators = function () {
    let generators = [
      "traditional",
      "padded",
      "uuid",
      "autoincrement"
    ];
    return generators;
  };

  return {
    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testAllowUserKeysTrueDefaultSharding: function () {
      generators().forEach((generator) => {
        let numShards = 2;
        if (generator === "autoincrement") {
          numShards = 1;
        }
        let c = db._create(cn, {keyOptions: {type: generator, allowUserKeys: true}, numberOfShards: numShards});
        try {
          let p = c.properties();
          assertTrue(p.keyOptions.allowUserKeys);
          assertEqual(generator, p.keyOptions.type);

          let doc = c.insert({_key: "1234"});
          assertEqual("1234", doc._key);
          assertEqual(1, c.count());
          doc = c.document("1234");
          assertEqual("1234", doc._key);

          doc = c.insert({});
          assertMatch(/^[a-zA-z0-9]+/, doc._key);
          assertEqual(2, c.count());
          assertTrue(c.exists(doc._key));
        } finally {
          db._drop(cn);
        }
      });
    },

    testAllowUserKeysFalseDefaultSharding: function () {
      generators().forEach((generator) => {
        let numShards = 2;
        if (generator === "autoincrement") {
          numShards = 1;
        }
        let c = db._create(cn, {keyOptions: {type: generator, allowUserKeys: false}, numberOfShards: numShards});
        try {
          let p = c.properties();
          assertFalse(p.keyOptions.allowUserKeys);
          assertEqual(generator, p.keyOptions.type);

          c.insert({_key: "1234"});
          fail();
        } catch (err) {
          assertEqual(0, c.count());
          assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
            err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);

          let doc = c.insert({});
          assertMatch(/^[a-zA-z0-9]+/, doc._key);
          assertEqual(1, c.count());
          assertTrue(c.exists(doc._key));
        } finally {
          db._drop(cn);
        }
      });
    },

    testAllowUserKeysTrueCustomSharding: function () {
      if (!cluster) {
        return;
      }

      generators().forEach((generator) => {
        let numShards = 2;
        if (generator === "autoincrement") {
          numShards = 1;
        }
        let c = db._create(cn, {
          shardKeys: ["value"],
          keyOptions: {type: generator, allowUserKeys: true},
          numberOfShards: numShards
        });
        try {
          let p = c.properties();
          assertTrue(p.keyOptions.allowUserKeys);
          assertEqual(generator, p.keyOptions.type);

          c.insert({_key: "1234"});
          fail();
        } catch (err) {
          assertEqual(0, c.count());
          assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
            err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);

          let doc = c.insert({});
          assertMatch(/^[a-zA-z0-9]+/, doc._key);
          assertEqual(1, c.count());
          assertTrue(c.exists(doc._key));
        } finally {
          db._drop(cn);
        }
      });
    },

    testAllowUserKeysFalseCustomSharding: function () {
      if (!cluster) {
        return;
      }

      generators().forEach((generator) => {
        let numShards = 2;
        if (generator === "autoincrement") {
          numShards = 1;
        }
        let c = db._create(cn, {
          shardKeys: ["value"],
          keyOptions: {type: generator, allowUserKeys: false},
          numberOfShards: numShards
        });
        try {
          let p = c.properties();
          assertFalse(p.keyOptions.allowUserKeys);
          assertEqual(generator, p.keyOptions.type);

          c.insert({_key: "1234"});
          fail();
        } catch (err) {
          assertEqual(0, c.count());
          assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
            err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);

          let doc = c.insert({});
          assertMatch(/^[a-zA-z0-9]+/, doc._key);
          assertEqual(1, c.count());
          assertTrue(c.exists(doc._key));
        } finally {
          db._drop(cn);
        }
      });
    },

  };
}

function PersistedLastValueSuite() {
  'use strict';

  let generators = function () {
    let generators = [
      "padded",
      "autoincrement"
    ];
    return generators;
  };

  return {
    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testPersistedLastValue: function () {
      generators().forEach((generator) => {
        let numShards = 2;
        if (generator === "autoincrement") {
          numShards = 1;
        }
        let c = db._create(cn, {keyOptions: {type: generator}, numberOfShards: numShards});
        try {
          let p = c.properties();
          assertTrue(p.keyOptions.allowUserKeys);
          assertEqual("number", typeof p.keyOptions.lastValue, generator);
          let lastValue = p.keyOptions.lastValue;
          assertEqual(0, lastValue);

          for (let i = 0; i < 10; ++i) {
            c.insert({});
            p = c.properties();
            let newLastValue = p.keyOptions.lastValue;
            if (Number(numShards) === 2) {
              assertTrue(newLastValue > lastValue);
            }
            lastValue = newLastValue;
          }
        } finally {
          db._drop(cn);
        }
      });
    },

  };
}

function KeyGenerationLocationSuite() {
  'use strict';

  let generators = function () {
    let generators = [
      "traditional",
      "padded",
      "uuid",
      "autoincrement"
    ];
    return generators;
  };

  return {
    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      IM.debugClearFailAt();
      db._drop(cn);
    },

    testThatFailurePointsWork1: function () {
      if (!cluster) {
        return;
      }

      let c = db._create(cn, {keyOptions: {type: "traditional"}, numberOfShards: 1});

      IM.debugSetFailAt("KeyGenerator::generateOnSingleServer", filter);
      try {
        c.insert({});
      } catch (err) {
        assertEqual(ERRORS.ERROR_DEBUG.code, err.errorNum);
      }
    },

    testThatFailurePointsWork2: function () {
      if (!cluster) {
        return;
      }

      let c = db._create(cn, {keyOptions: {type: "traditional"}, numberOfShards: 2});

      IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);
      try {
        c.insert({});
      } catch (err) {
        assertEqual(ERRORS.ERROR_DEBUG.code, err.errorNum);
      }
    },

    testSingleShardInserts: function () {
      if (!cluster) {
        return;
      }

      // fail if we generate a key on a coordinator
      IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);

      generators().forEach((generator) => {
        let c = db._create(cn, {keyOptions: {type: generator}, numberOfShards: 1});
        try {
          // test various ways of inserting documents into the collection

          // single insert, using document API
          c.insert({});

          // batch insert, using document API
          c.insert([{}, {}, {}]).forEach((res) => {
            assertFalse(res.hasOwnProperty('error'));
          });

          // single insert, using AQL
          db._query(`INSERT
          {} INTO
          ${cn}`);

          // batch insert, using AQL
          db._query(`FOR i IN 1..3 INSERT {} INTO ${cn}`);

          assertEqual(8, c.count());
        } finally {
          db._drop(cn);
        }
      });
    },

    testSingleShardInsertsCustomSharding: function () {
      if (!cluster) {
        return;
      }

      // fail if we generate a key on a coordinator
      IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);

      generators().forEach((generator) => {
        let c = db._create(cn, {keyOptions: {type: generator}, numberOfShards: 1, shardKeys: ["id"]});
        try {
          // test various ways of inserting documents into the collection

          // single insert, using document API
          c.insert({id: "123"});

          // batch insert, using document API
          c.insert([{id: "123"}, {id: "123"}, {id: "123"}]).forEach((res) => {
            assertFalse(res.hasOwnProperty('error'));
          });

          // single insert, using AQL
          db._query(`INSERT
          { id: "123" } INTO
          ${cn}`);

          // batch insert, using AQL
          db._query(`FOR i IN 1..3 INSERT { id: "123" } INTO ${cn}`);

          assertEqual(8, c.count());
        } finally {
          db._drop(cn);
        }
      });
    },

    testOneShardInserts: function () {
      if (!isEnterprise || !cluster) {
        // can test OneShard only in EE
        return;
      }

      // fail if we generate a key on a coordinator
      IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);

      db._createDatabase(cn, {sharding: "single"});
      try {
        db._useDatabase(cn);
        assertEqual("single", db._properties().sharding);

        generators().forEach((generator) => {
          let c = db._create(cn, {keyOptions: {type: generator}});
          assertEqual(1, c.properties().numberOfShards);

          try {
            // test various ways of inserting documents into the collection

            // single insert, using document API
            c.insert({});

            // batch insert, using document API
            c.insert([{}, {}, {}]).forEach((res) => {
              assertFalse(res.hasOwnProperty('error'));
            });

            // single insert, using AQL
            db._query(`INSERT
            {} INTO
            ${cn}`);

            // batch insert, using AQL
            db._query(`FOR i IN 1..3 INSERT {} INTO ${cn}`);

            assertEqual(8, c.count());
          } finally {
            db._drop(cn);
          }
        });
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },

    testMultiShardInserts: function () {
      if (!cluster) {
        return;
      }

      // fail if we generate a key on a DB server
      IM.debugSetFailAt("KeyGenerator::generateOnSingleServer", filter);

      generators().forEach((generator) => {
        if (generator === "autoincrement") {
          return;
        }
        let c = db._create(cn, {keyOptions: {type: generator}, numberOfShards: 2});
        try {
          // test various ways of inserting documents into the collection

          // single insert, using document API
          c.insert({});

          // batch insert, using document API
          c.insert([{}, {}, {}]).forEach((res) => {
            assertFalse(res.hasOwnProperty('error'));
          });

          // single insert, using AQL
          db._query(`INSERT
          {} INTO
          ${cn}`);

          // batch insert, using AQL
          db._query(`FOR i IN 1..3 INSERT {} INTO ${cn}`);

          assertEqual(8, c.count());
        } finally {
          db._drop(cn);
        }
      });
    },

    testMultiShardInsertsCustomSharding: function () {
      if (!cluster) {
        return;
      }

      // fail if we generate a key on a DB server
      IM.debugSetFailAt("KeyGenerator::generateOnSingleServer", filter);

      generators().forEach((generator) => {
        if (generator === "autoincrement") {
          return;
        }
        let c = db._create(cn, {keyOptions: {type: generator}, numberOfShards: 2, shardKeys: ["id"]});
        try {
          // test various ways of inserting documents into the collection

          // single insert, using document API
          c.insert({});

          // with custom shard key value
          c.insert({id: "123"});

          // batch insert, using document API
          c.insert([{}, {}, {}]);

          // with custom shard key value
          c.insert([{id: "123"}, {id: "123"}, {id: "123"}]).forEach((res) => {
            assertFalse(res.hasOwnProperty('error'));
          });

          // single insert, using AQL
          db._query(`INSERT
          {} INTO
          ${cn}`);

          // with custom shard key value
          db._query(`INSERT
          {id: "123"} INTO
          ${cn}`);

          // batch insert, using AQL
          db._query(`FOR i IN 1..3 INSERT {} INTO ${cn}`);

          // with custom shard key value
          db._query(`FOR i IN 1..3 INSERT {id: "123"} INTO ${cn}`);

          assertEqual(16, c.count());
        } finally {
          db._drop(cn);
        }
      });
    },

  };
}

function KeyGenerationLocationSmartGraphSuite() {
  'use strict';

  // Note: No key generators defined here in this suite compared to the other one
  // sitting in this test file. This is totally valid, because one cannot define
  // manually which key generator can be used in a Graph/SmartGraph/EnterpriseGraph etc.

  return {
    setUp: function () {
      try {
        graphs._drop(gn, true);
      } catch (err) {
      }
    },

    tearDown: function () {
      IM.debugClearFailAt();
      try {
        graphs._drop(gn, true);
      } catch (err) {
      }
    },

    testSingleShardSmartVertexInserts: function () {
      if (cluster) {
        // fail if we generate a key on a coordinator
        IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);
      } else {
        // single server: we can actually get here with the SmartGraph simulator!
        IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);
      }

      // note: test can run in single server as well!
      const smartGraphProperties = {
        numberOfShards: 1,
        smartGraphAttribute: "value"
      };
      runSmartVertexInserts(smartGraphProperties);
    },

    testSingleShardEnterpriseVertexInserts: function () {
      if (!cluster) {
        // single server: we can actually get here with the SmartGraph simulator!
        IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);
      }
      // Note: To cluster tests and why we're not setting any failure point here:
      // We cannot declare a failure point here because both, that means:
      // * SingleServer || DatabaseServer key generation is valid
      // * Coordinator-based key generation is valid as well.
      // This statement is only true in case we're using a SingleShard(!).
      // Therefore, the other test below (MultiShardEnterprise) will include
      // these assertions for both cases (cluster and singleserver).

      // note: test can run in single server as well!
      const enterpriseGraphProperties = {
        numberOfShards: 1,
        isSmart: true
      };
      runSmartVertexInserts(enterpriseGraphProperties);
    },

    testMultiShardSmartVertexInserts: function () {
      if (cluster) {
        // fail if we generate a key on a DB server
        IM.debugSetFailAt("KeyGenerator::generateOnSingleServer", filter);
      } else {
        // single server: we can actually get here with the SmartGraph simulator!
        IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);
      }

      // note: test can run in single server as well!
      const smartGraphProperties = {
        numberOfShards: 2,
        smartGraphAttribute: "value"
      };
      runSmartVertexInserts(smartGraphProperties);
    },

    testMultiShardEnterpriseVertexInserts: function () {
      if (cluster) {
        // fail if we generate a key on a DB server
        IM.debugSetFailAt("KeyGenerator::generateOnSingleServer", filter);
      } else {
        // single server: we can actually get here with the SmartGraph simulator!
        IM.debugSetFailAt("KeyGenerator::generateOnCoordinator", filter);
      }

      // note: test can run in single server as well!
      const enterpriseGraphProperties = {
        numberOfShards: 2,
        isSmart: true
      };
      runSmartVertexInserts(enterpriseGraphProperties);
    },

    testSingleShardSmartEdgeInserts: function () {
      // note: test can run in single server as well!
      const smartGraphProperties = {
        numberOfShards: 1,
        smartGraphAttribute: "value"
      };
      runSmartEdgeInserts(smartGraphProperties);
    },

    testMultiShardSmartEdgeInserts: function () {
      // note: test can run in single server as well!
      const smartGraphProperties = {
        numberOfShards: 1,
        smartGraphAttribute: "value"
      };
      runSmartEdgeInserts(smartGraphProperties);
    },

    testSingleShardEnterpriseEdgeInserts: function () {
      // note: test can run in single server as well!
      const enterpriseGraphProperties = {
        numberOfShards: 1,
        isSmart: true
      };
      runSmartEdgeInserts(enterpriseGraphProperties);
    },

    testMultiShardEnterpriseEdgeInserts: function () {
      // note: test can run in single server as well!
      const enterpriseGraphProperties = {
        numberOfShards: 1,
        isSmart: true
      };
      runSmartEdgeInserts(enterpriseGraphProperties);
    },

  };
}

jsunity.run(KeyGeneratorsSuite);
jsunity.run(TraditionalSuite);
jsunity.run(AutoIncrementSuite);
jsunity.run(AllowUserKeysSuite);
jsunity.run(PersistedLastValueSuite);

if (IM.debugCanUseFailAt()) {
  jsunity.run(KeyGenerationLocationSuite);
}
if (isEnterprise && IM.debugCanUseFailAt()) {
  jsunity.run(KeyGenerationLocationSmartGraphSuite);
}

return jsunity.done();
