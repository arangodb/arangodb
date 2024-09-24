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
const db = arangodb.db;
const ERRORS = arangodb.errors;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual, fail} = jsunity.jsUnity.assertions;
const _ = require("lodash");

function multiDimSparseTestSuite() {
  const collectionName = 'UnitTestMdiIndexCollection';
  const indexName = 'UnitTestMdiIndex';
  const database = 'UnitTestMdiIndexDB';

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
    },
    tearDownAll: function () {
      db._useDatabase('_system');
      db._dropDatabase(database);
    },
    tearDown: function () {
      const c = db[collectionName];
      if (c !== undefined) {
        c.drop();
      }
    },

    testMultiDimNonSparse: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi',
        name: indexName,
        fields: ["x", "y"],
        sparse: false,
        fieldValueTypes: 'double'
      });
      assertFalse(idx.sparse);

      const idx2 = c.index(indexName);
      assertFalse(idx2.sparse);

      // Inserting a document with missing values for x or y should return an error

      const expectErrorOnInsert = function (doc) {
        try {
          c.save(doc);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, err.errorNum);
        }
      };

      expectErrorOnInsert({});
      expectErrorOnInsert({x: 5});
      expectErrorOnInsert({y: 5});
      c.save({x: 7, y: 5});
    },

    testMultiDimSparse: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi',
        name: indexName,
        fields: ["x", "y"],
        sparse: true,
        fieldValueTypes: 'double'
      });
      assertTrue(idx.sparse);

      const idx2 = c.index(indexName);
      assertTrue(idx2.sparse);

      // Inserting a document with missing values for x or y should simply ignore the document for the index
      c.save([{}, {x: 5}, {y: 5}, {x: 7, y: 5}]);

      {
        const figures = c.figures(true);
        assertEqual(4, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }

      // Test deletion
      const doc1 = c.save({x: 8, y: 10});
      const doc2 = c.save({x: 8});

      {
        const figures = c.figures(true);
        assertEqual(6, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 2));
      }

      c.remove([doc1, doc2]);

      {
        const figures = c.figures(true);
        assertEqual(4, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }
    },

    testMultiDimNonSparseUnique: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi',
        name: indexName,
        fields: ["x", "y"],
        sparse: false,
        fieldValueTypes: 'double',
        unique: true,
      });
      assertFalse(idx.sparse);
      assertTrue(idx.unique);

      const idx2 = c.index(indexName);
      assertFalse(idx2.sparse);
      assertTrue(idx2.unique);

      // Inserting a document with missing values for x or y should return an error

      const expectErrorOnInsert = function (doc) {
        try {
          c.save(doc);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, err.errorNum);
        }
      };

      expectErrorOnInsert({});
      expectErrorOnInsert({x: 5});
      expectErrorOnInsert({y: 5});
      c.save({x: 7, y: 5});
    },

    testMultiDimSparseUnique: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi',
        name: indexName,
        fields: ["x", "y"],
        sparse: true,
        fieldValueTypes: 'double',
        unique: true,
      });
      assertTrue(idx.sparse);
      assertTrue(idx.unique);

      const idx2 = c.index(indexName);
      assertTrue(idx2.sparse);
      assertTrue(idx2.unique);

      // Inserting a document with missing values for x or y should simply ignore the document for the index
      c.save([{}, {x: 5}, {y: 5}, {x: 7, y: 5}]);

      {
        const figures = c.figures(true);
        assertEqual(4, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }

      // Test deletion
      const doc1 = c.save({x: 8, y: 10});
      const doc2 = c.save({x: 8});

      {
        const figures = c.figures(true);
        assertEqual(6, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 2));
      }

      c.remove([doc1, doc2]);

      {
        const figures = c.figures(true);
        assertEqual(4, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }
    },

    testMdiSparseQuery: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi',
        name: indexName,
        fields: ["x", "y"],
        storedValues: ["z"],
        sparse: true,
        fieldValueTypes: 'double',
      });

      for (let k = 0; k < 10; k++) {
        c.insert({x: k, y: k, z: k});
      }

      // For MDI with sparse, we can only use doc.x < something if the query also includes something like doc.x != NULL
      const queries = [
        [`FOR doc IN ${collectionName} FILTER doc.x < 0 && doc.y > 10 && doc.x != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.x > 0 && doc.y > 10 && doc.x != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.x > 0 && doc.y < 10 && doc.y != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.x > 0 && doc.y > 10 && doc.y != NULL && doc.x != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.x < 0 && doc.y < 10 && doc.y != NULL && doc.x != NULL RETURN doc.z`],
      ];

      for (const [query] of queries) {
        const plan = db._createStatement({query}).explain().plan;

        const [indexNode] = plan.nodes.filter(n => n.type === 'IndexNode');
        assertTrue(indexNode.filter === undefined, query);
      }
    }
  };
}

function multiDimPrefixedSparseTestSuite() {
  const collectionName = 'UnitTestMdiIndexCollection';
  const indexName = 'UnitTestMdiIndex';
  const database = 'UnitTestMdiIndexDB';

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
    },
    tearDownAll: function () {
      db._useDatabase('_system');
      db._dropDatabase(database);
    },
    tearDown: function () {
      const c = db[collectionName];
      if (c !== undefined) {
        c.drop();
      }
    },

    testMultiDimPrefixedNonSparse: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi-prefixed',
        name: indexName,
        fields: ["x", "y"],
        sparse: false,
        fieldValueTypes: 'double',
        prefixFields: ["attr"],
      });
      assertFalse(idx.sparse);

      const idx2 = c.index(indexName);
      assertFalse(idx2.sparse);

      // Inserting a document with missing values for x or y should return an error
      const expectErrorOnInsert = function (doc) {
        try {
          c.save(doc);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_QUERY_INVALID_ARITHMETIC_VALUE.code, err.errorNum);
        }
      };

      expectErrorOnInsert({});
      expectErrorOnInsert({x: 5});
      expectErrorOnInsert({y: 5});
      c.save({x: 7, y: 5});
      c.save({x: 7, y: 5, attr: "foo"});

      // missing prefix fields should be indexed as null
      {
        const res = db._query(aql`FOR d IN ${c} FILTER d.attr == NULL AND d.x > 0 RETURN d`).toArray();
        assertEqual(1, res.length);
      }

      const doc1 = c.save({x: 17, y: -3, attr: null});
      const doc2 = c.save({x: 17, y: -3, attr: "foo"});
      const doc3 = c.save({x: 17, y: -3});

      {
        const res = db._query(aql`FOR d IN ${c} FILTER d.attr == NULL AND d.x > 0 RETURN d`).toArray();
        assertEqual(3, res.length);
      }

      c.remove([doc1, doc2, doc3]);

      {
        const res = db._query(aql`FOR d IN ${c} FILTER d.attr == NULL AND d.x > 0 RETURN d`).toArray();
        assertEqual(1, res.length);
      }
    },

    testMultiDimPrefixedSparse: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi-prefixed',
        name: indexName,
        fields: ["x", "y"],
        sparse: true,
        fieldValueTypes: 'double',
        prefixFields: ["attr"],
      });
      assertTrue(idx.sparse);

      const idx2 = c.index(indexName);
      assertTrue(idx2.sparse);

      // Inserting a document with missing values for attr, x or y should simply ignore the document for the index
      c.save([{}, {x: 5}, {y: 5}, {x: 7, y: 5}, {x: 7, y: 5, attr: "foo"}, {x: 7, y: 5, attr: null}]);

      {
        const figures = c.figures(true);
        assertEqual(6, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }

      // Test deletion
      const doc1 = c.save({x: 8, y: 10, attr: "foo"});
      const doc2 = c.save({x: 8, y: 10});
      const doc3 = c.save({x: 8,  attr: "foo"});

      {
        const figures = c.figures(true);
        assertEqual(9, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 2));
      }

      c.remove([doc1, doc2, doc3]);

      {
        const figures = c.figures(true);
        assertEqual(6, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }
    },

    testMultiDimPrefixedNonSparseUnique: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi-prefixed',
        name: indexName,
        fields: ["x", "y"],
        sparse: false,
        fieldValueTypes: 'double',
        unique: true,
        prefixFields: ["attr"],
      });
      assertFalse(idx.sparse);
      assertTrue(idx.unique);

      const idx2 = c.index(indexName);
      assertFalse(idx2.sparse);
      assertTrue(idx2.unique);

      // Inserting a document with missing values for x or y should return an error

      const expectErrorOnInsert = function (doc, which = ERRORS.ERROR_QUERY_INVALID_ARITHMETIC_VALUE) {
        try {
          c.save(doc);
          fail();
        } catch (err) {
          assertEqual(which.code, err.errorNum);
        }
      };

      expectErrorOnInsert({});
      expectErrorOnInsert({x: 5});
      expectErrorOnInsert({y: 5});
      c.save({x: 7, y: 5});
      c.save({x: 7, y: 5, attr: "foo"});

      // missing prefix fields should be indexed as null
      {
        const res = db._query(aql`FOR d IN ${c} FILTER d.attr == NULL AND d.x > 0 RETURN d`).toArray();
        assertEqual(1, res.length);
      }

      const doc1 = c.save({x: 17, y: -3, attr: null});
      const doc2 = c.save({x: 17, y: -3, attr: "foo"});
      // unset field `attr` is evaluated to `null` and thus violates unique constraint because of doc1
      expectErrorOnInsert({x: 17, y: -3}, ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);

      {
        const res = db._query(aql`FOR d IN ${c} FILTER d.attr == NULL AND d.x > 0 RETURN d`).toArray();
        assertEqual(2, res.length);
      }

      c.remove([doc1, doc2]);

      {
        const res = db._query(aql`FOR d IN ${c} FILTER d.attr == NULL AND d.x > 0 RETURN d`).toArray();
        assertEqual(1, res.length);
      }
    },

    testMultiDimPrefixedSparseUnique: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi-prefixed',
        name: indexName,
        fields: ["x", "y"],
        sparse: true,
        fieldValueTypes: 'double',
        unique: true,
        prefixFields: ["attr"],
      });
      assertTrue(idx.sparse);
      assertTrue(idx.unique);

      const idx2 = c.index(indexName);
      assertTrue(idx2.sparse);
      assertTrue(idx2.unique);

      // Inserting a document with missing values for attr, x or y should simply ignore the document for the index
      c.save([{}, {x: 5}, {y: 5}, {x: 7, y: 5}, {x: 7, y: 5, attr: "foo"}, {x: 7, y: 5, attr: null}]);

      {
        const figures = c.figures(true);
        assertEqual(6, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }

      // Test deletion
      const doc1 = c.save({x: 8, y: 10, attr: "foo"});
      const doc2 = c.save({x: 8, y: 10});
      const doc3 = c.save({x: 8,  attr: "foo"});

      {
        const figures = c.figures(true);
        assertEqual(9, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 2));
      }

      c.remove([doc1, doc2, doc3]);

      {
        const figures = c.figures(true);
        assertEqual(6, figures.engine.documents);
        assertTrue(figures.engine.indexes.filter(x => x.type === 'mdi').every(x => x.count === 1));
      }
    },


    testMdiPrefixedSparseQuery: function () {
      const c = db._create(collectionName);
      const idx = c.ensureIndex({
        type: 'mdi-prefixed',
        name: indexName,
        fields: ["x", "y"],
        storedValues: ["z"],
        prefixFields: ["a"],
        sparse: true,
        fieldValueTypes: 'double',
      });

      for (let k = 0; k < 10; k++) {
        c.insert({x: k, y: k, z: k});
      }

      // For MDI with sparse, we can only use doc.x < something if the query also includes something like doc.x != NULL
      const queries = [
        [`FOR doc IN ${collectionName} FILTER doc.a == 12 && doc.x < 0 && doc.y > 10 && doc.x != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.a == 12 && doc.x > 0 && doc.y > 10 && doc.x != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.a == 12 && doc.x > 0 && doc.y < 10 && doc.y != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.a == 12 && doc.x > 0 && doc.y > 10 && doc.y != NULL && doc.x != NULL RETURN doc.z`],
        [`FOR doc IN ${collectionName} FILTER doc.a == 12 && doc.x < 0 && doc.y < 10 && doc.y != NULL && doc.x != NULL RETURN doc.z`],
      ];

      for (const [query] of queries) {
        const plan = db._createStatement({query}).explain().plan;

        const [indexNode] = plan.nodes.filter(n => n.type === 'IndexNode');
        assertTrue(indexNode.filter === undefined, query);
      }
    }
  };
}

jsunity.run(multiDimSparseTestSuite);
jsunity.run(multiDimPrefixedSparseTestSuite);

return jsunity.done();
