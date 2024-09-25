/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull */

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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const { helper, versionHas } = require("@arangodb/test-helper");
const platform = require('internal').platform;

const cn = "UnitTestsCollection";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: basics
////////////////////////////////////////////////////////////////////////////////

function IndexSuite() {
  'use strict';

  let collection = null;

  return {

    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: indexes
////////////////////////////////////////////////////////////////////////////////

    testIndexes: function() {
      var res = collection.indexes();

      assertEqual(1, res.length);

      collection.ensureIndex({type: "geo", fields: ["a"]});
      collection.ensureIndex({type: "geo", fields: ["a", "b"]});

      res = collection.indexes();

      assertEqual(3, res.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get indexes
////////////////////////////////////////////////////////////////////////////////

    testGetIndexes: function() {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      collection.ensureIndex({type: "geo", fields: ["a"]});
      collection.ensureIndex({type: "geo", fields: ["a", "b"]});

      res = collection.getIndexes();

      assertEqual(3, res.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get index
////////////////////////////////////////////////////////////////////////////////

    testIndex: function() {
      var id = collection.ensureIndex({type: "geo", fields: ["a"]});

      var idx = collection.index(id.id);
      assertEqual(id.id, idx.id);

      idx = collection.index(id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id.id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id);
      assertEqual(id.id, idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get index by name
////////////////////////////////////////////////////////////////////////////////

    testIndexByName: function() {
      var id = collection.ensureIndex({type: "geo", fields: ["a"]});

      var idx = collection.index(id.name);
      assertEqual(id.id, idx.id);
      assertEqual(id.name, idx.name);

      var fqn = `${collection.name()}/${id.name}`;
      idx = internal.db._index(fqn);
      assertEqual(id.id, idx.id);
      assertEqual(id.name, idx.name);

      idx = collection.index(fqn);
      assertEqual(id.id, idx.id);
      assertEqual(id.name, idx.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop index
////////////////////////////////////////////////////////////////////////////////

    testDropIndex: function() {
      var id = collection.ensureIndex({type: "geo", fields: ["a"]});
      var res = collection.dropIndex(id.id);
      assertTrue(res);

      try {
        collection.dropIndex(id.id);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }

      id = collection.ensureIndex({type: "geo", fields: ["a"]});
      res = collection.dropIndex(id);
      assertTrue(res);

      try {
        collection.dropIndex(id);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }

      id = collection.ensureIndex({type: "geo", fields: ["a"]});
      res = internal.db._dropIndex(id);
      assertTrue(res);

      try {
        collection.dropIndex(id);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop index by id string
////////////////////////////////////////////////////////////////////////////////

    testDropIndexString: function() {
      // pick up the numeric part (starts after the slash)
      var id = collection.ensureIndex({type: "geo", fields: ["a"]}).id.substr(cn.length + 1);
      var res = collection.dropIndex(collection.name() + "/" + id);
      assertTrue(res);

      try {
        collection.dropIndex(collection.name() + "/" + id);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }

      id = collection.ensureIndex({type: "geo", fields: ["a"]}).id.substr(cn.length + 1);
      res = collection.dropIndex(parseInt(id, 10));
      assertTrue(res);

      try {
        collection.dropIndex(parseInt(id, 10));
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }

      id = collection.ensureIndex({type: "geo", fields: ["a"]}).id.substr(cn.length + 1);
      res = internal.db._dropIndex(collection.name() + "/" + id);
      assertTrue(res);

      try {
        collection.dropIndex(collection.name() + "/" + id);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop index by id string
////////////////////////////////////////////////////////////////////////////////

    testDropIndexByName: function() {
      // pick up the numeric part (starts after the slash)
      var name = collection.ensureIndex({type: "geo", fields: ["a"]}).name;
      var res = collection.dropIndex(collection.name() + "/" + name);
      assertTrue(res);

      try {
        collection.dropIndex(collection.name() + "/" + name);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }

      name = collection.ensureIndex({type: "geo", fields: ["a"]}).name;
      res = collection.dropIndex(name);
      assertTrue(res);

      try {
        collection.dropIndex(name);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }

      name = collection.ensureIndex({type: "geo", fields: ["a"]}).name;
      res = internal.db._dropIndex(collection.name() + "/" + name);
      assertTrue(res);

      try {
        collection.dropIndex(collection.name() + "/" + name);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access a non-existing index
////////////////////////////////////////////////////////////////////////////////

    testGetNonExistingIndexes: function() {
      ["2444334000000", "9999999999999"].forEach(function(id) {
        try {
          collection.index(id);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access an existing index of an unloaded collection
////////////////////////////////////////////////////////////////////////////////

    testGetIndexUnloaded: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["test"]});

      helper.waitUnload(collection);

      assertEqual(idx.id, collection.index(idx.id).id);
      assertEqual(idx.id, collection.getIndexes()[1].id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access an existing index of a dropped collection
////////////////////////////////////////////////////////////////////////////////

    testGetIndexDropped: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["test"]});

      collection.drop();

      if (versionHas('coverage')) {
        internal.wait(2, false);
      }
      try {
        collection.index(idx.id);
        fail();
      } catch (e1) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, e1.errorNum);
      }

      try {
        collection.getIndexes();
        fail();
      } catch (e2) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, e2.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create index with field names that start or end with ":"
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidField: function() {
      const indexTypes = ["geo", "fulltext", "persistent", "inverted"];
      const isUnique = [true, false];
      const invalidFields = [":value", "value:"];

      collection.save({value: 1});

      indexTypes.forEach(indexType => {
        isUnique.forEach(isUnique => {
          invalidFields.forEach(invalidField => {
            try {
              if (indexType === "inverted") {
                collection.ensureIndex({type: indexType, unique: isUnique, fields: [{"name": invalidField}]});
              } else {
                collection.ensureIndex({type: indexType, unique: isUnique, fields: [invalidField]});
              }
              fail();
            } catch (err) {
              assertEqual(errors.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED.code, err.errorNum);
            }
          });
        });
      });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: return value of getIndexes
////////////////////////////////////////////////////////////////////////////////

function GetIndexesSuite() {
  'use strict';

  let collection = null;

  return {

    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get primary
////////////////////////////////////////////////////////////////////////////////

    testGetPrimary: function() {
      var res = collection.getIndexes();

      assertEqual(1, res.length);
      var idx = res[0];

      assertEqual("primary", idx.type);
      assertTrue(idx.unique);
      assertEqual(["_key"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetPersistentUnique1: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"], unique: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetPersistentUnique2: function() {
      collection.ensureIndex({type: "persistent", fields: ["value1", "value2"], unique: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetSparsePersistentUnique1: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"], unique: true, sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetSparsePersistentUnique2: function() {
      collection.ensureIndex({type: "persistent", fields: ["value1", "value2"], unique: true, sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetPersistentNonUnique1: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"]});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetPersistentNonUnique2: function() {
      collection.ensureIndex({type: "persistent", fields: ["value1", "value2"]});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetSparsePersistentNonUnique1: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"], sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testGetSparsePersistentNonUnique2: function() {
      collection.ensureIndex({type: "persistent", fields: ["value1", "value2"], sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation
////////////////////////////////////////////////////////////////////////////////

    testCreationPersistentMixedSparsity: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["a"], sparse: true});
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["a"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureIndex({type: "persistent", fields: ["a"], sparse: false});

      assertNotEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["a"], idx.fields);
      assertTrue(idx.isNewlyCreated);
      id = idx.id;

      idx = collection.ensureIndex({type: "persistent", fields: ["a"], sparse: false});

      assertEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["a"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: permuted attributes
////////////////////////////////////////////////////////////////////////////////

    testCreationPermutedAttributes: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["a", "b"]});
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["a", "b"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureIndex({type: "persistent", fields: ["b", "a"]});

      assertNotEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["b", "a"], idx.fields);
      assertTrue(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get fulltext index
////////////////////////////////////////////////////////////////////////////////

    testGetFulltext: function() {
      collection.ensureIndex({type: "fulltext", fields: ["value"]});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("fulltext", idx.type);
      assertFalse(idx.unique);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBUnique1: function() {
      collection.ensureIndex({type: "persistent", unique: true, fields: ["value"]});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBUnique2: function() {
      collection.ensureIndex({type: "persistent", unique: true, fields: ["value1", "value2"]});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBUnique1: function() {
      collection.ensureIndex({type: "persistent", unique: true, fields: ["value"], sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBUnique2: function() {
      collection.ensureIndex({type: "persistent", unique: true, fields: ["value1", "value2"], sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBNonUnique1: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"]});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBNonUnique2: function() {
      collection.ensureIndex({type: "persistent", fields: ["value1", "value2"]});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBNonUnique1: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"], sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value"], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBNonUnique2: function() {
      collection.ensureIndex({type: "persistent", fields: ["value1", "value2"], sparse: true});
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value1", "value2"], idx.fields);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: return value of getIndexes for an edge collection
////////////////////////////////////////////////////////////////////////////////

function GetIndexesEdgesSuite() {
  'use strict';

  let collection = null;

  return {

    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._createEdgeCollection(cn);
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get primary
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetPrimary: function() {
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[0];

      assertEqual("primary", idx.type);
      assertTrue(idx.unique);
      assertEqual(["_key"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get edge
////////////////////////////////////////////////////////////////////////////////

    testGetEdge: function() {
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("edge", idx.type);
      assertFalse(idx.unique);
      assertEqual(["_from", "_to"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoConstraint1: function() {
      collection.ensureIndex({type: "geo", fields: ["lat", "lon"], geoJson: false});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoConstraint2: function() {
      collection.ensureIndex({type: "geo", fields: ["lat", "lon"], geoJson: true});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoConstraint3: function() {
      collection.ensureIndex({type: "geo", fields: ["lat"], geoJson: true, sparse: true});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.geoJson);
      assertTrue(idx.sparse);
      assertEqual(["lat"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoIndex1: function() {
      collection.ensureIndex({type: "geo", fields: ["lat"], geoJson: true, sparse: true});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.geoJson);
      assertTrue(idx.sparse);
      assertEqual(["lat"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoIndex2: function() {
      collection.ensureIndex({type: "geo", fields: ["lat", "lon"]});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["lat", "lon"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetPersistentUnique: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"], unique: true});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique persistent index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetSparsePersistentUnique: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"], unique: true, sparse: true});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get persistent index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetPersistent: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"]});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["value"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get persistent index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetSparsePersistent: function() {
      collection.ensureIndex({type: "persistent", fields: ["value"], sparse: true});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["value"], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get fulltext index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetFulltext: function() {
      collection.ensureIndex({type: "fulltext", fields: ["value"]});
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("fulltext", idx.type);
      assertFalse(idx.unique);
      assertEqual(["value"], idx.fields);
      assertEqual(2, idx.minLength);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    }

  };
}

function DuplicateValuesSuite() {
  'use strict';

  let collection = null;

  return {

    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown: function() {
      collection = internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessTopAttribute: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: null});
      collection.save({a: 0});
      collection.save({a: 1});
      collection.save({a: 2});
      try {
        collection.save({a: 2});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessSubAttribute: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a.b"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a.b"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: {b: null}});
      collection.save({a: {b: 0}});
      collection.save({a: {b: 1}});
      collection.save({a: {b: 2}});
      try {
        collection.save({a: {b: 2}});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessSubAttributeKey: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a._key"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a._key"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: {_key: null}});
      collection.save({a: {_key: 0}});
      collection.save({a: {_key: 1}});
      collection.save({a: {_key: 2}});
      try {
        collection.save({a: {_key: 2}});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessArrayAttribute: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a[*].b"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: [{b: null}]});
      collection.save({a: [{b: 0}]});
      collection.save({a: [{b: 1}]});
      collection.save({a: [{b: 2}]});
      try {
        collection.save({a: [{b: 2}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessArrayAttributeKey: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a[*]._key"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a[*]._key"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: [{_key: null}]});
      collection.save({a: [{_key: 0}]});
      collection.save({a: [{_key: 1}]});
      collection.save({a: [{_key: 2}]});
      try {
        collection.save({a: [{_key: 2}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testDeduplicationDefault: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a[*].b"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertTrue(idx.deduplicate);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: [{b: 1}, {b: 1}]});
      collection.save({a: [{b: 2}, {b: 2}]});
      collection.save({a: [{b: 3}, {b: 4}]});
      try {
        collection.save({a: [{b: 2}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(3, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testDeduplicationOn: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a[*].b"], deduplicate: true});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertTrue(idx.deduplicate);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: [{b: 1}, {b: 1}]});
      collection.save({a: [{b: 2}, {b: 2}]});
      collection.save({a: [{b: 3}, {b: 4}]});
      try {
        collection.save({a: [{b: 2}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(3, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testDeduplicationOff: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a[*].b"], deduplicate: false});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertFalse(idx.deduplicate);
      assertTrue(idx.isNewlyCreated);

      collection.save({a: [{b: 1}]});
      collection.save({a: [{b: 2}]});
      collection.save({a: [{b: 3}, {b: 4}]});
      try {
        collection.save({a: [{b: 5}, {b: 5}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(3, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation
////////////////////////////////////////////////////////////////////////////////

    testCreationUniqueConstraint: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["a"]});
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["a"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureIndex({type: "persistent", fields: ["a"]});

      assertEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual(["a"], idx.fields);
      assertFalse(idx.isNewlyCreated);

      idx = collection.ensureIndex({type: "persistent", fields: ["a"], sparse: true});

      assertNotEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["a"], idx.fields);
      assertTrue(idx.isNewlyCreated);
      id = idx.id;

      idx = collection.ensureIndex({type: "persistent", fields: ["a"], sparse: true});

      assertEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual(["a"], idx.fields);
      assertFalse(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: permuted attributes
////////////////////////////////////////////////////////////////////////////////

    testCreationPermutedUniqueConstraint: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["a", "b"]});
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual(["a", "b"].sort(), idx.fields.sort());
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureIndex({type: "persistent", fields: ["b", "a"]});

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual(["a", "b"].sort(), idx.fields.sort());
      assertNotEqual(id, idx.id);
      assertTrue(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniqueDocuments: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["a", "b"]});

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual(["a", "b"].sort(), idx.fields.sort());
      assertTrue(idx.isNewlyCreated);

      collection.save({a: 1, b: 1});
      collection.save({a: 1, b: 1});

      collection.save({a: 1});
      collection.save({a: 1});
      collection.save({a: null, b: 1});
      collection.save({a: null, b: 1});
      collection.save({c: 1});
      collection.save({c: 1});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniqueDocumentsSparseIndex: function() {
      var idx = collection.ensureIndex({type: "persistent", fields: ["a", "b"], sparse: true});

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual(["a", "b"].sort(), idx.fields.sort());
      assertTrue(idx.isNewlyCreated);

      collection.save({a: 1, b: 1});
      collection.save({a: 1, b: 1});

      collection.save({a: 1});
      collection.save({a: 1});
      collection.save({a: null, b: 1});
      collection.save({a: null, b: 1});
      collection.save({c: 1});
      collection.save({c: 1});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolation1: function() {
      collection.ensureIndex({type: "persistent", fields: ["a"], unique: true});
      collection.ensureIndex({type: "persistent", fields: ["b"]});

      collection.save({a: "test1", b: 1});
      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err1) {
      }

      var doc1 = collection.save({a: "test2", b: 1});
      assertNotEqual(doc1._key, "");

      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err2) {
      }

      var doc2 = collection.save({a: "test3", b: 1});
      assertNotEqual(doc2._key, "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolationSparse1: function() {
      collection.ensureIndex({type: "persistent", fields: ["a"], unique: true, sparse: true});
      collection.ensureIndex({type: "persistent", fields: ["b"], sparse: true});

      collection.save({a: "test1", b: 1});
      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err1) {
      }

      var doc1 = collection.save({a: "test2", b: 1});
      assertNotEqual(doc1._key, "");

      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err2) {
      }

      var doc2 = collection.save({a: "test3", b: 1});
      assertNotEqual(doc2._key, "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolation2: function() {
      collection.ensureIndex({type: "persistent", fields: ["a"], unique: true});
      collection.ensureIndex({type: "persistent", fields: ["b"]});

      collection.save({a: "test1", b: 1});
      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err1) {
      }

      var doc1 = collection.save({a: "test2", b: 1});
      assertNotEqual(doc1._key, "");

      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err2) {
      }

      var doc2 = collection.save({a: "test3", b: 1});
      assertNotEqual(doc2._key, "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolationSparse2: function() {
      collection.ensureIndex({type: "persistent", fields: ["a"], unique: true, sparse: true});
      collection.ensureIndex({type: "persistent", fields: ["b"], sparse: true});

      collection.save({a: "test1", b: 1});
      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err1) {
      }

      var doc1 = collection.save({a: "test2", b: 1});
      assertNotEqual(doc1._key, "");

      try {
        collection.save({a: "test1", b: 1});
        fail();
      } catch (err2) {
      }

      var doc2 = collection.save({a: "test3", b: 1});
      assertNotEqual(doc2._key, "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessAndLookup: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["value"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["value"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      const bound = 1000;

      let docs = [];
      for (let i = -bound; i < bound; ++i) {
        docs.push({value: i});
      }
      collection.insert(docs);

      internal.db._executeTransaction({
        collections: {write: cn},
        action: function(params) {
          // need to run compaction in the rocksdb case, as the lookups
          // may use bloom filters afterwards but not for memtables
          require("internal").db[params.cn].compact();
        },
        params: {cn}
      });

      assertEqual(2 * bound, collection.count());

      for (let i = -bound; i < bound; ++i) {
        let docs = collection.byExample({value: i}).toArray();
        assertEqual(1, docs.length);
        assertEqual(i, docs[0].value);

        collection.update(docs[0]._key, docs[0]);
      }

      for (let i = -bound; i < bound; ++i) {
        try {
          collection.insert({value: i});
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
        }
      }
    },

    testUniquenessAndLookup2: function() {
      var idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["value"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["value"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      let i = 0;
      while (i < 100000) {
        let docs = [];
        for (let j = 0; j < 20; ++j) {
          docs.push({value: i++});
        }
        collection.insert(docs);
        i *= 2;
      }

      internal.db._executeTransaction({
        collections: {write: cn},
        action: function(params) {
          // need to run compaction in the rocksdb case, as the lookups
          // may use bloom filters afterwards but not for memtables
          require("internal").db[params.cn].compact();
        },
        params: {cn}
      });

      i = 0;
      while (i < 100000) {
        for (let j = 0; j < 20; ++j) {
          let docs = collection.byExample({value: i}).toArray();
          assertEqual(1, docs.length);
          assertEqual(i, docs[0].value);
          collection.update(docs[0]._key, docs[0]);
          ++i;
        }
        i *= 2;
      }
    },

    testUniqueIndexNullSubattribute: function() {
      let idx = collection.ensureIndex({type: "persistent", unique: true, fields: ["a.b"]});

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a.b"], idx.fields);
      assertTrue(idx.isNewlyCreated);

      // as "a" is null here, "a.b" should also be null, at least it should not fail when accessing it via the index
      collection.insert({_key: "test", a: null});
      collection.update("test", {something: "test2"});

      let doc = collection.document("test");
      assertNull(doc.a);
      assertEqual("test2", doc.something);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: test multi-index rollback
////////////////////////////////////////////////////////////////////////////////

function MultiIndexRollbackSuite() {
  'use strict';

  let collection = null;

  return {

    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._createEdgeCollection(cn);
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rollback on index insertion
////////////////////////////////////////////////////////////////////////////////

    testIndexRollback: function() {
      collection.ensureIndex({type: "persistent", fields: ["_from", "_to", "link"], unique: true});
      collection.ensureIndex({type: "persistent", fields: ["_to", "ext"], unique: true, sparse: true});

      var res = collection.getIndexes();

      assertEqual(4, res.length);
      assertEqual("primary", res[0].type);
      assertEqual("edge", res[1].type);
      assertEqual("persistent", res[2].type);
      assertEqual("persistent", res[3].type);

      const docs = [
        {"_from": "fromC/a", "_to": "toC/1", "link": "one"},
        {"_from": "fromC/b", "_to": "toC/1", "link": "two"},
        {"_from": "fromC/c", "_to": "toC/1", "link": "one"}
      ];

      collection.insert(docs);
      assertEqual(3, collection.count());

      try {
        internal.db._query('FOR doc IN [ {_from: "fromC/a", _to: "toC/1", link: "one", ext: 2337789}, {_from: "fromC/b", _to: "toC/1", link: "two", ext: 2337799}, {_from: "fromC/c", _to: "toC/1", link: "one", ext: 2337789} ] UPSERT {_from: doc._from, _to: doc._to, link: doc.link} INSERT { _from: doc._from, _to: doc._to, link: doc.link, ext: doc.ext} UPDATE {ext: doc.ext} IN ' + collection.name());
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      res = internal.db._query("FOR doc IN " + collection.name() + " FILTER doc._to == 'toC/1' RETURN doc._from").toArray();
      assertEqual(3, res.length);
    }

  };
}

function ParallelIndexSuite() {
  'use strict';
  let tasks = require("@arangodb/tasks");

  return {

    setUp: function() {
      internal.db._drop(cn);
      internal.db._create(cn);
    },

    tearDown: function() {
      let rounds = 0;
      while(true) {
        const stillRunning = tasks.get().filter(function(task) {
          return (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/)); });
        if(stillRunning.length === 0) {
          break;
        }
        require("internal").wait(0.5, false);
        rounds++;
        if(rounds % 10 === 0) {
          console.log("After %s rounds there are still the following tasks %s", rounds, JSON.stringify(stillRunning));
        }
      }
      internal.db._drop(cn);
    },

    testCreateInParallel: function() {
      // maximum concurrency for index creation. we need to limit the number
      // here because otherwise the server may be overwhelmed by too many
      // concurrent index creations being in progress.
      const maxThreads = 7;
      const noIndexes = versionHas('coverage') ? 40 : 80;

      let time = require("internal").time;
      let start = time();
      while (true) {
        let indexes = require("internal").db._collection(cn).getIndexes();
        assertTrue(indexes.length >= 1, indexes);
        if (indexes.length === noIndexes + 1) {
          // primary index + user-defined indexes
          break;
        }
        for (let i = indexes.length - 1; i < Math.min(noIndexes, indexes.length - 1 + maxThreads); ++i) {
          let command = 'require("internal").db._collection("' + cn + '").ensureIndex({ type: "persistent", fields: ["value' + i + '"] });';
          tasks.register({name: "UnitTestsIndexCreate" + i, command: command});
        }
        if (time() - start > 180) {
          // wait for 3 minutes maximum
          fail("Timeout creating " + noIndexes + " indices after 3 minutes: " + JSON.stringify(indexes));
        }
        require("internal").wait(0.5, false);
      }

      let indexes = require("internal").db._collection(cn).getIndexes();
      assertEqual(noIndexes + 1, indexes.length);
    },

    testCreateInParallelDuplicate: function() {
      // maximum concurrency for index creation. we need to limit the number
      // here because otherwise the server may be overwhelmed by too many
      // concurrent index creations being in progress.
      const maxThreads = 7;
      const noIndexes = 100;

      let time = require("internal").time;
      let start = time();
      while (true) {
        let indexes = require("internal").db._collection(cn).getIndexes();
        if (indexes.length === 4 + 1) {
          // primary index + user-defined indexes
          break;
        }
        for (let i = indexes.length - 1; i < Math.min(noIndexes, indexes.length - 1 + maxThreads); ++i) {
          let command = 'require("internal").db._collection("' + cn + '").ensureIndex({ type: "persistent", fields: ["value' + (i % 4) + '"] });';
          tasks.register({name: "UnitTestsIndexCreate" + i, command: command});
        }
        if (time() - start > 180) {
          // wait for 3 minutes maximum
          fail("Timeout creating indices after 3 minutes: " + JSON.stringify(indexes));
        }
        require("internal").wait(0.5, false);
      }

      // wait some extra time because we just have 4 distinct indexes
      // these will be created relatively quickly. by waiting here a bit
      // we give the other pending tasks a chance to execute too (but they
      // will not do anything because the target indexes already exist)
      require("internal").wait(5, false);

      let indexes = require("internal").db._collection(cn).getIndexes();
      assertEqual(4 + 1, indexes.length);
    }
  };
}

function IndexUpdateSuite() {
  'use strict';

  return {

    setUp: function() {
      internal.db._drop(cn);
      internal.db._create(cn, { replicationFactor: 2 });
    },

    tearDown: function() {
      internal.db._drop(cn);
    },

    testUpdateCollectionPropertiesWithView: function() {
      let view = internal.db._createView(cn + "View", "arangosearch", {});

      try {
        view.properties({ links: { [cn] : { includeAllFields: true } } });

        const schema = {
          rule: { 
            properties: { nums: { type: "array", items: { type: "number", maximum: 6 } } }, 
            additionalProperties: { type: "string" },
            required: ["nums"]
          },
          level: "moderate",
          message: "The document does not contain an array of numbers in attribute 'nums', or one of the numbers is greater than 6."
        };

        internal.db[cn].properties({ cacheEnabled: true, schema }); 

        let props = internal.db[cn].properties();
        assertTrue(props.cacheEnabled, props);
        let s = props.schema;
        delete s.type;
        assertEqual(schema, s);
      } finally {
        internal.db._dropView(cn + "View");
      }
    }
  };
}

jsunity.run(IndexSuite);
jsunity.run(GetIndexesSuite);
jsunity.run(GetIndexesEdgesSuite);
jsunity.run(DuplicateValuesSuite);
jsunity.run(MultiIndexRollbackSuite);
jsunity.run(ParallelIndexSuite);
jsunity.run(IndexUpdateSuite);

return jsunity.done();
