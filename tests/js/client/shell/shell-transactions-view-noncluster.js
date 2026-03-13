/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, fail */

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
/// @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const analyzers = require("@arangodb/analyzers");
const internal = require("internal");
const ERRORS = arangodb.errors;
const db = arangodb.db;
const isEnterprise = require("internal").isEnterprise();

const qqWithSync = `FOR doc IN UnitTestsView
                      SEARCH ANALYZER(doc.text IN TOKENS('the quick brown', 'myText'), 'myText')
                      OPTIONS { waitForSync : true }
                      SORT TFIDF(doc)
                      LIMIT 4
                      RETURN doc`;

const qq = `FOR doc IN UnitTestsView
              SEARCH ANALYZER(doc.text IN TOKENS('the quick brown', 'myText'), 'myText')
              SORT TFIDF(doc)
              LIMIT 4
              RETURN doc`;

const qqIndexWithSync = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true}
              FILTER TOKENS('the quick brown', 'myText') any in doc.text
              SORT doc._key ASC
              LIMIT 4
              RETURN doc`;

const qqSearchAliasWithSync = `FOR doc IN searchAliasView
              SEARCH ANALYZER(doc.text IN TOKENS('the quick brown', 'myText'), 'myText')
              OPTIONS { waitForSync : true }
              SORT TFIDF(doc)
              LIMIT 4
              RETURN doc`;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TransactionsIResearchSuite() {
  'use strict';
  let c = null;
  const cn = 'UnitTestsCollection';
  let view = null;
  let saView = null;

  return {

    setUpAll: function() {
      analyzers.save(
        "myText",
        "text",
        { locale: "en.UTF-8", stopwords: [ ] },
        [ "frequency", "norm", "position" ]
      );
    },

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      // we need try...catch here because at least one test drops the collection itself!
      try {
        c.unload();
        c.drop();
      } catch (err) {
      }
      c = null;
      if (view) {
        view.drop();
        view = null;
      }
      if (saView) {
        saView.drop();
        saView = null;
      }
      internal.wait(0.0);
    },

    tearDownAll: function () {
      analyzers.remove('myText');
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief should honor rollbacks of inserts
    ////////////////////////////////////////////////////////////////////////////
    testRollbackInsertWithLinks1 : function () {

      let indexMeta = {};
      let viewMeta = {};
      if (isEnterprise) {
        viewMeta = {links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          {name: 'text', analyzer: 'myText'}
        ]};
      } else {
        viewMeta = {links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }}}}};
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value[*]"},
          {name: 'text', analyzer: 'myText'}
        ]};
      }
      view = db._createView("UnitTestsView", "arangosearch", {});
      c.ensureIndex(indexMeta);
      view.properties(viewMeta);
      saView = db._createView("searchAliasView", "search-alias", {indexes:[{collection: "UnitTestsCollection", index: "inverted"}]});
      let links = view.properties().links;
      assertNotEqual(links[cn], undefined);

      c.save({ _key: "full", text: "the quick brown fox jumps over the lazy dog" });
      c.save({ _key: "half", text: "quick fox over lazy" });
      c.save({ name_1: "123", "value": [{ "nested_1": [{ "nested_2": "foo123"}]}], text1: "quick fox is lazy"});

      const trx = db._createTransaction({ collections: { write: cn } });
      try {
        const tc = trx.collection(cn);
        tc.save({ _key: "other_half", text: "the brown jumps the dog" });
        tc.save({ _key: "quarter", text: "quick over" });
        tc.save({ name_1: "456", "value": [{ "nested_1": [{ "nested_2": "123"}]}], text1: "lazy fox is lazy"});
        trx.abort();
      } catch (err) {
        trx.abort();
        throw err;
      }

      let result = db._query(qqWithSync).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'full');

      result = db._query(qqSearchAliasWithSync).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'full');

      result = db._query(qqIndexWithSync).toArray();
      assertEqual(result.length, 2); // we don't track order for inverted index
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief should honor rollbacks of inserts
    ////////////////////////////////////////////////////////////////////////////
    testRollbackInsertWithLinks2 : function () {
      c.ensureIndex({type: 'hash', fields:['val', 'text1'], unique: true});

      let indexMeta = {};
      let viewMeta = {};
      if (isEnterprise) {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}} } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          {name: 'text', analyzer: 'myText'}
        ]};
      } else {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] } } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value[*]"},
          {name: 'text', analyzer: 'myText'}
        ]};
      }
      c.ensureIndex(indexMeta);
      view = db._createView("UnitTestsView", "arangosearch", {});
      view.properties(viewMeta);
      saView = db._createView("searchAliasView", "search-alias", {indexes:[{collection: "UnitTestsCollection", index: "inverted"}]});
      let links = view.properties().links;
      assertNotEqual(links[cn], undefined);

      const trx = db._createTransaction({ collections: { write: cn } });
      try {
        const tc = trx.collection(cn);
        tc.save({ _key: "full", text: "the quick brown fox jumps over the lazy dog", val: 1 });
        tc.save({ _key: "half", text: "quick fox over lazy", val: 2 });
        tc.save({ _key: "other_half", text: "the brown jumps the dog", val: 3 });
        tc.save({ _key: "a", name_1: "456", "value": [{ "nested_1": [{ "nested_2": "123"}]}], text1: "lazy fox is crazy"});
        tc.save({ _key: "b", name_1: "123", "value": [{ "nested_1": [{ "nested_2": "321"}]}], text1: "crazy fox is crazy"});

        try {
          tc.save({ _key: "quarter", text: "quick over", val: 3 }); // unique constraint violation
          tc.save({ _key: "c", name_1: "123", "value": [{ "nested_1": [{ "nested_2": "a"}]}]});
          fail();
        } catch(err) {
          assertEqual(err.errorNum, ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
        }

        trx.commit();
      } catch (err) {
        trx.abort();
        throw err;
      }

      let result = db._query(qqWithSync).toArray();
      assertEqual(result.length, 3);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'other_half');
      assertEqual(result[2]._key, 'full');

      result = db._query(qqSearchAliasWithSync).toArray();
      assertEqual(result.length, 3);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'other_half');
      assertEqual(result[2]._key, 'full');

      result = db._query(qqIndexWithSync).toArray();
      assertEqual(result.length, 3); // we don't track order for inverted index
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief should honor rollbacks of inserts
    ////////////////////////////////////////////////////////////////////////////
    testRollbackInsertWithLinks3 : function () {
      let indexMeta = {};
      let viewMeta = {};
      if (isEnterprise) {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}} } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          {name: 'text', analyzer: 'myText'}
        ]};
      } else {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": {} } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value[*]"},
          {name: 'text', analyzer: 'myText'}
        ]};
      }
      c.ensureIndex(indexMeta);
      view = db._createView("UnitTestsView", "arangosearch", {});
      view.properties(viewMeta);
      saView = db._createView("searchAliasView", "search-alias", {indexes:[{collection: "UnitTestsCollection", index: "inverted"}]});
      let links = view.properties().links;
      assertNotEqual(links[cn], undefined);

      c.ensureIndex({type: 'hash', fields:['val', 'text1'], unique: true});

      const trx = db._createTransaction({ collections: { write: cn } });
      try {
        const tc = trx.collection(cn);
        tc.save({ _key: "full", text: "the quick brown fox jumps over the lazy dog", val: 1 });
        tc.save({ _key: "half", text: "quick fox over lazy", val: 2 });
        tc.save({ _key: "other_half", text: "the brown jumps the dog", val: 3 });
        tc.save({ _key: "1", name_1: "123", "value": [{ "nested_1": [{ "nested_2": "a"}]}]});
        tc.save({ _key: "2", name_1: "456", "value": [{ "nested_1": [{ "nested_2": "123"}]}], text1: "lazy fox is crazy"});
        tc.save({ _key: "3", name_1: "123", "value": [{ "nested_1": [{ "nested_2": "321"}]}], text1: "crazy fox is crazy"});

        try {
          tc.save({ _key: "quarter", text: "quick over", val: 3 }); // unique constraint violation
          fail();
        } catch(err) {
          assertEqual(err.errorNum, ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
        }

        trx.commit();
      } catch (err) {
        trx.abort();
        throw err;
      }

      let result = db._query(qqWithSync).toArray();
      assertEqual(result.length, 3);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'other_half');
      assertEqual(result[2]._key, 'full');

      result = db._query(qqSearchAliasWithSync).toArray();
      assertEqual(result.length, 3);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'other_half');
      assertEqual(result[2]._key, 'full');

      result = db._query(qqIndexWithSync).toArray();
      assertEqual(result.length, 3); // we don't track order for inverted index
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief should honor rollbacks of removals
    ////////////////////////////////////////////////////////////////////////////
    testRollbackRemovalWithLinks1 : function () {
      c.ensureIndex({type: 'hash', fields:['val', 'text1'], unique: true});

      let indexMeta = {};
      let viewMeta = {};
      if (isEnterprise) {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}} } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          {name: 'text', analyzer: 'myText'}
        ]};
      } else {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": { } } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value[*]"},
          {name: 'text', analyzer: 'myText'}
        ]};
      }
      c.ensureIndex(indexMeta);
      view = db._createView("UnitTestsView", "arangosearch", {});
      view.properties(viewMeta);
      saView = db._createView("searchAliasView", "search-alias", {indexes:[{collection: "UnitTestsCollection", index: "inverted"}]});
      let links = view.properties().links;
      assertNotEqual(links[cn], undefined);

      c.save({ _key: "full", text: "the quick brown fox jumps over the lazy dog", val: 1 });
      c.save({ _key: "half", text: "quick fox over lazy", val: 2 });
      c.save({ _key: "other_half", text: "the brown jumps the dog", val: 3 });
      c.save({ _key: "quarter", text: "quick quick over", val: 4 });
      c.save({ name_1: "123", "value": [{ "nested_1": [{ "nested_2": "a"}]}]});
      c.save({ name_1: "456", "value": [{ "nested_1": [{ "nested_2": "123"}]}], text1: "lazy fox is crazy"});
      c.save({ name_1: "123", "value": [{ "nested_1": [{ "nested_2": "321"}]}], text1: "crazy fox is crazy"});

      const trx = db._createTransaction({ collections: { write: cn } });
      try {
        const tc = trx.collection(cn);
        tc.remove("full");
        tc.remove("half");
        tc.remove("other_half");
        tc.remove("quarter");
        trx.abort();
      } catch (err) {
        trx.abort();
        throw err;
      }

      let result = db._query(qqWithSync).toArray();
      assertEqual(result.length, 4);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'quarter');
      assertEqual(result[2]._key, 'other_half');
      assertEqual(result[3]._key, 'full');

      result = db._query(qqSearchAliasWithSync).toArray();
      assertEqual(result.length, 4);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'quarter');
      assertEqual(result[2]._key, 'other_half');
      assertEqual(result[3]._key, 'full');

      result = db._query(qqIndexWithSync).toArray();
      assertEqual(result.length, 4); // we don't track order for inverted index
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief waitForSync queries must be rejected inside a transaction
    ////////////////////////////////////////////////////////////////////////////
    testWaitForSyncError : function () {
      c.ensureIndex({type: 'hash', fields:['val', 'text'], unique: true});

      let indexMeta = {};
      let viewMeta = {};
      if (isEnterprise) {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}} } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
          {"name": 'text', "analyzer": 'myText'}
        ]};
      } else {
        viewMeta = { links: { 'UnitTestsCollection' : { fields: {text: {analyzers: [ "myText" ] }, "value": { } } } } };
        indexMeta = { type: 'inverted', name: 'inverted', fields: [
          {"name": "value[*]"},
          {name: 'text', analyzer: 'myText'}
        ]};
      }
      c.ensureIndex(indexMeta);
      view = db._createView("UnitTestsView", "arangosearch", {});
      view.properties(viewMeta);
      saView = db._createView("searchAliasView", "search-alias", {indexes:[{collection: "UnitTestsCollection", index: "inverted"}]});
      let links = view.properties().links;
      assertNotEqual(links[cn], undefined);

      c.save({ _key: "full", text: "the quick brown fox jumps over the lazy dog", val: 1 });
      c.save({ _key: "half", text: "quick fox over lazy", val: 2 });
      c.save({ _key: "other_half", text: "the brown jumps the dog", val: 3 });
      c.save({ _key: "quarter", text: "quick quick over", val: 4 });
      c.save({ name_1: "123", "value": [{ "nested_1": [{ "nested_2": "a"}]}], val: 5 });
      c.save({ name_1: "456", "value": [{ "nested_1": [{ "nested_2": "123"}]}], text: "lazy fox is crazy", val: 6 });
      c.save({ name_1: "123", "value": [{ "nested_1": [{ "nested_2": "321"}]}], text: "crazy fox is crazy", val: 7 });

      // it should not be possible to run a waitForSync query inside a streaming transaction
      {
        const trx = db._createTransaction({ collections: { write: cn } });
        try {
          trx.collection(cn).remove("full");
          trx.collection(cn).remove("half");
          try {
            trx.query(qqWithSync);
            fail();
          } catch(err) {
            assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
          }
        } finally {
          trx.abort();
        }
      }

      {
        const trx = db._createTransaction({ collections: { write: cn } });
        try {
          trx.collection(cn).remove("full");
          trx.collection(cn).remove("half");
          try {
            trx.query(qqSearchAliasWithSync);
            fail();
          } catch(err) {
            assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
          }
        } finally {
          trx.abort();
        }
      }

      {
        const trx = db._createTransaction({ collections: { write: cn } });
        try {
          trx.collection(cn).remove("full");
          trx.collection(cn).remove("half");
          try {
            trx.query(qqIndexWithSync);
            fail();
          } catch(err) {
            assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
          }
        } finally {
          trx.abort();
        }
      }

      let result = db._query(qqWithSync).toArray();
      assertEqual(result.length, 4);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'quarter');
      assertEqual(result[2]._key, 'other_half');
      assertEqual(result[3]._key, 'full');

      result = db._query(qqSearchAliasWithSync).toArray();
      assertEqual(result.length, 4);
      assertEqual(result[0]._key, 'half');
      assertEqual(result[1]._key, 'quarter');
      assertEqual(result[2]._key, 'other_half');
      assertEqual(result[3]._key, 'full');

      result = db._query(qqIndexWithSync).toArray();
      assertEqual(result.length, 4); // we don't track order for inverted index
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TransactionsIResearchSuite);

return jsunity.done();
