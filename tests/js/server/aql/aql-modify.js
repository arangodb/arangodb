/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, assertMatch, fail, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, modification blocks
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Michael Hackstein
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const db = require("@arangodb").db;
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const errors = internal.errors;

const collectionName = "UnitTestAqlModify";
let col;

////////////////////////////////////////////////////////////////////////////////
/// @brief helper functions
////////////////////////////////////////////////////////////////////////////////

let invCounter = 0;
const genInvalidValue = function () {
  ++invCounter;
  return `invalid${invCounter}`;
};

const setUp = function () {
  tearDown();
  col = db._create(collectionName, { numberOfShards: 3 });
  let list = [];
  for (let i = 0; i < 2000; ++i) {
    list.push({ val: i });
  }
  col.save(list);
  assertEqual(2000, col.count());
};

const tearDown = function () {
  db._drop(collectionName);
  col = null;
};

const buildSetOfDocs = function (count = 0) {
  if (count <= 0 || count >= 2000) {
    return new Set(col.toArray());
  }
  const query = `FOR d IN ${collectionName} LIMIT ${count} RETURN d`;
  return new Set(db._query(query).toArray());
};

const validateDocsAreUpdated = function (docs, invalid, areUpdated) {
  for (let d of docs) {
    const nowStored = col.document(d._key);
    if (areUpdated) {
      assertEqual(invalid, nowStored.val, `Did not update ${JSON.stringify(d)} using wrong _rev value`);
    } else {
      assertNotEqual(invalid, nowStored.val, `Updated ${JSON.stringify(d)} using wrong _rev value`);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlUpdateOptionsSuite() {

  return {
    setUp, tearDown,

    testUpdateSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        try {
          db._query(q, { key: d._key });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
        }
      }
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpdateManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      try {
        db._query(q, { docs: [...docs] });
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpdateEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpdateSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateSingleWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: @rev, val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key, rev: d._rev });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateManyWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs
               UPDATE {_key: doc._key, _rev: doc._rev, val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateEnumerationWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
               UPDATE {_key: doc._key, _rev: doc._rev, val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },
  };
};

function aqlUpdateWithOptionsSuite() {

  return {
    setUp, tearDown,

    testUpdateWithSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: "invalid"} WITH {val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        try {
          db._query(q, { key: d._key });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
        }
      }
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpdateWithManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key, _rev: "invalid"} WITH {val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      try {
        db._query(q, { docs: [...docs] });
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpdateWithEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key, _rev: "invalid"} WITH {val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpdateWithSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: "invalid"} WITH {val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key, _rev: "invalid"} WITH {val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key, _rev: "invalid"} WITH {val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: "invalid"} WITH {val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key, _rev: "invalid"} WITH {val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key, _rev: "invalid"} WITH {val: "${invalid}"}
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithSingleWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key, _rev: @rev}
               WITH {val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key, rev: d._rev });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithManyWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs
               UPDATE {_key: doc._key, _rev: doc._rev}
               WITH {val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithEnumerationWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
               UPDATE {_key: doc._key, _rev: doc._rev}
               WITH {val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },
  };
};

function aqlUpdateWithRevOptionsSuite() {

  return {
    setUp, tearDown,

    testUpdateWithRevSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key} WITH {_rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key} WITH {_rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key} WITH {_rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key} WITH {_rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key} WITH {_rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key} WITH {_rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `UPDATE {_key: @key} WITH {_rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPDATE {_key: doc._key} WITH {_rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpdateWithRevEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPDATE {_key: doc._key} WITH {_rev: "invalid", val: "${invalid}"}
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    }
  };
};

function aqlReplaceOptionsSuite() {

  return {
    setUp, tearDown,

    testReplaceSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        try {
          db._query(q, { key: d._key });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
        }
      }
      validateDocsAreUpdated(docs, invalid, false);
    },

    testReplaceManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      try {
        db._query(q, { docs: [...docs] });
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testReplaceEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testReplaceSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceSingleWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: @rev, val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key, rev: d._rev });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceManyWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs
               REPLACE {_key: doc._key, _rev: doc._rev, val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceEnumerationWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
               REPLACE {_key: doc._key, _rev: doc._rev, val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

  };
};

function aqlReplaceWithOptionsSuite() {

  return {
    setUp, tearDown,

    testReplaceWithSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: "invalid"} WITH { val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        try {
          db._query(q, { key: d._key });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
        }
      }
      validateDocsAreUpdated(docs, invalid, false);
    },

    testReplaceWithManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key, _rev: "invalid"} WITH { val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      try {
        db._query(q, { docs: [...docs] });
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testReplaceWithEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key, _rev: "invalid"} WITH { val: "${invalid}" }
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      validateDocsAreUpdated(docs, invalid, false);
    },

    testReplaceWithSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: "invalid"} WITH { val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key, _rev: "invalid"} WITH { val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key, _rev: "invalid"} WITH { val: "${invalid}" }
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: "invalid"} WITH { val: "${invalid}" } IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key, _rev: "invalid"} WITH { val: "${invalid}" } IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key, _rev: "invalid"} WITH { val: "${invalid}" }
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithSingleWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key, _rev: @rev}
               WITH {val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key, rev: d._rev });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithManyWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs
               REPLACE {_key: doc._key, _rev: doc._rev}
               WITH {val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithEnumerationWithValidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
               REPLACE {_key: doc._key, _rev: doc._rev}
               WITH {val: "${invalid}"}
               IN ${collectionName}
               OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

  };
};

function aqlReplaceWithRevOptionsSuite() {

  return {
    setUp, tearDown,

    testReplaceWithRevSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key} WITH { _rev: "invalid", val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key} WITH { _rev: "invalid", val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key} WITH { _rev: "invalid", val: "${invalid}" }
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key} WITH { _rev: "invalid", val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key} WITH { _rev: "invalid", val: "${invalid}" } IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key} WITH { _rev: "invalid", val: "${invalid}" }
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `REPLACE {_key: @key} WITH { _rev: "invalid", val: "${invalid}" } IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REPLACE {_key: doc._key} WITH { _rev: "invalid", val: "${invalid}" } IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      validateDocsAreUpdated(docs, invalid, true);
    },

    testReplaceWithRevEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REPLACE {_key: doc._key} WITH { _rev: "invalid", val: "${invalid}" }
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      validateDocsAreUpdated(docs, invalid, true);
    }

  };
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlRemoveOptionsSuite() {

  return {
    setUp, tearDown,

    testRemoveSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `REMOVE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        try {
          db._query(q, { key: d._key });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
        }
      }
      assertEqual(2000, col.count(), `We removed the document`);
    },

    testRemoveManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REMOVE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      try {
        db._query(q, { docs: [...docs] });
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }
      assertEqual(2000, col.count(), `We removed the document`);
    },

    testRemoveEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REMOVE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }
      assertEqual(2000, col.count(), `We removed the document`);
    },

    testRemoveSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `REMOVE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      assertEqual(2000 - 1, col.count(), `We did not remove the document`);
    },

    testRemoveManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REMOVE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      assertEqual(2000 - 10, col.count(), `We did not remove the document`);
    },

    testRemoveEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REMOVE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      assertEqual(0, col.count(), `We did not remove the document`);
    },

    testRemoveSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `REMOVE {_key: @key, _rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      assertEqual(2000 - 1, col.count(), `We did not remove the document`);
    },

    testRemoveManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs REMOVE {_key: doc._key, _rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      assertEqual(2000 - 10, col.count(), `We did not remove the document`);
    },

    testRemoveEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                REMOVE {_key: doc._key, _rev: "invalid", val: "${invalid}"}
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      assertEqual(0, col.count(), `We did not remove the document`);
    }
  };
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlUpsertOptionsSuite() {

  return {
    setUp, tearDown,

    testUpsertSingleWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `UPSERT {_key: @key} INSERT {} UPDATE {_rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        try {
          db._query(q, { key: d._key });
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
        }
      }
      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpsertManyWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPSERT {_key: doc._key} INSERT {} UPDATE {_rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      try {
        db._query(q, { docs: [...docs] });
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpsertEnumerationWithInvalidRev: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPSERT {_key: doc._key} INSERT {}
                UPDATE {_rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpsertSingleWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `UPSERT {_key: @key} INSERT {}
               UPDATE {_rev: "invalid", val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertManyWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPSERT {_key: doc._key} INSERT {}
               UPDATE {_rev: "invalid", val: "${invalid}"} IN ${collectionName}
               OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertEnumerationWithInvalidRevIgnore: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPSERT {_key: doc._key} INSERT {}
                UPDATE {_rev: "invalid", val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertSingleWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `UPSERT {_key: @key} INSERT {} UPDATE {_rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, { key: d._key });
      }
      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertManyWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPSERT {_key: doc._key} INSERT {}
                UPDATE {_rev: "invalid", val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, { docs: [...docs] });

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertEnumerationWithInvalidRevDefault: function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPSERT {_key: doc._key} INSERT {}
                UPDATE {_rev: "invalid", val: "${invalid}"}
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    /* We cannot yet solve this. If you need to ensure _rev value checks put them in the UPDATE {} clause
    testUpsertSingleWithInvalidRevInMatch : function () {
      const invalid = genInvalidValue();
      let q = `UPSERT {_key: @key, _rev: "invalid"} INSERT {} UPDATE {val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      db._explain(q, {key: "1"});
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        try {
          db._query(q, {key: d._key});
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
        }
      }
      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpsertManyWithInvalidRevInMatch : function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPSERT {_key: doc._key, _rev: "invalid"} INSERT {} UPDATE {val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs(10);
      try {
        db._query(q, {docs: [...docs]});
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpsertEnumerationWithInvalidRevInMatch : function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPSERT {_key: doc._key, _rev: "invalid"} INSERT {}
                UPDATE {val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: false}`;
      let docs = buildSetOfDocs();
      try {
        db._query(q);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_CONFLICT.code);
      }

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, false);
    },

    testUpsertSingleWithInvalidRevIgnoreInMatch : function () {
      const invalid = genInvalidValue();
      let q = `UPSERT {_key: @key, _rev: "invalid"} INSERT {}
               UPDATE {val: "${invalid}"} IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, {key: d._key});
      }
      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertManyWithInvalidRevIgnoreInMatch : function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPSERT {_key: doc._key, _rev: "invalid"} INSERT {}
               UPDATE {val: "${invalid}"} IN ${collectionName}
               OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs(10);
      db._query(q, {docs: [...docs]});

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertEnumerationWithInvalidRevIgnoreInMatch : function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPSERT {_key: doc._key, _rev: "invalid"} INSERT {}
                UPDATE {val: "${invalid}"}
                IN ${collectionName} OPTIONS {ignoreRevs: true}`;
      let docs = buildSetOfDocs();
      db._query(q);

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertSingleWithInvalidRevDefaultInMatch : function () {
      const invalid = genInvalidValue();
      let q = `UPSERT {_key: @key, _rev: "invalid"} INSERT {} UPDATE {val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(1);
      for (let d of docs) {
        db._query(q, {key: d._key});
      }
      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertManyWithInvalidRevDefaultInMatch : function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN @docs UPSERT {_key: doc._key, _rev: "invalid"} INSERT {}
                UPDATE {val: "${invalid}"} IN ${collectionName}`;
      let docs = buildSetOfDocs(10);
      db._query(q, {docs: [...docs]});

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },

    testUpsertEnumerationWithInvalidRevDefaultInMatch : function () {
      const invalid = genInvalidValue();
      let q = `FOR doc IN ${collectionName}
                UPSERT {_key: doc._key, _rev: "invalid"} INSERT {}
                UPDATE {val: "${invalid}"}
                IN ${collectionName}`;
      let docs = buildSetOfDocs();
      db._query(q);

      assertEqual(2000, col.count(), `We falsely triggered an INSERT`);
      validateDocsAreUpdated(docs, invalid, true);
    },
    */
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlUpdateOptionsSuite);
jsunity.run(aqlUpdateWithOptionsSuite);
jsunity.run(aqlUpdateWithRevOptionsSuite);
jsunity.run(aqlReplaceOptionsSuite);
jsunity.run(aqlReplaceWithOptionsSuite);
jsunity.run(aqlReplaceWithRevOptionsSuite);
jsunity.run(aqlRemoveOptionsSuite);
jsunity.run(aqlUpsertOptionsSuite);
return jsunity.done();
