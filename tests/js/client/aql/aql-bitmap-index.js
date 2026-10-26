/* jshint esnext: true */
/* global AQL_EXPLAIN */

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
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, assertNotEqual, fail} = jsunity.jsUnity.assertions;
const errors = require('internal').errors;
const db = require('internal').db;
const _ = require('lodash');

const cn = 'UnitTestsBitmapIndex';
const allLabels = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'];

// deterministic pseudo random numbers
function makeRandom(seed) {
  let state = seed;
  return () => {
    state = (state * 1103515245 + 12345) % 2147483648;
    return state / 2147483648;
  };
}

function makeLabels(random) {
  let r = random();
  if (r < 0.03) {
    return undefined;
  }
  if (r < 0.06) {
    return 'a';
  }
  if (r < 0.08) {
    return [1, 2.0, 'a', 'a'];
  }
  let labels = [];
  for (let l of allLabels) {
    if (random() < 0.35) {
      labels.push(l);
    }
  }
  if (random() < 0.1) {
    labels.push(labels.length > 0 ? labels[0] : 'h');
  }
  return labels;
}

function BitmapIndexSuite(values) {
  let docs = {};

  let expected = (search) => {
    let keys = [];
    for (let [key, doc] of Object.entries(docs)) {
      if (Array.isArray(doc.labels) &&
          search.every((s) => doc.labels.some((l) => l === s))) {
        keys.push(key);
      }
    }
    return keys.sort();
  };

  let bitmapIndexNodes = (query, bindVars) => {
    let plan = db._createStatement({query, bindVars}).explain().plan;
    return plan.nodes.filter((n) => n.type === 'IndexNode' &&
                                    n.indexes.some((i) => i.type === 'bitmap'));
  };

  let query = (q, bindVars, trx) => {
    let cursor = trx === undefined ? db._query(q, bindVars) : trx.query(q, bindVars);
    return cursor.toArray().sort();
  };

  let check = (search) => {
    const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels RETURN d._key`;
    assertEqual(1, bitmapIndexNodes(q, {labels: search}).length, search);
    assertEqual(expected(search), query(q, {labels: search}), search);
    const fullScan = `FOR d IN ${cn} OPTIONS {disableIndex: true}
                        FILTER @labels ALL IN d.labels RETURN d._key`;
    assertEqual(query(fullScan, {labels: search}), query(q, {labels: search}), search);
  };

  return {
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn, {numberOfShards: 3});
      let random = makeRandom(4711);
      docs = {};
      let batch = [];
      for (let i = 0; i < 3000; ++i) {
        let doc = {_key: 'k' + i};
        let labels = makeLabels(random);
        if (labels !== undefined) {
          doc.labels = labels;
        }
        docs[doc._key] = doc;
        batch.push(doc);
      }
      c.insert(batch);
      let idx = c.ensureIndex({type: 'bitmap', fields: ['labels'], values});
      assertEqual('bitmap', idx.type);
      assertEqual(values, idx.values);
      assertEqual(['labels'], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testCreateRejectsUniqueAndSparse: function () {
      for (let def of [{unique: true}, {sparse: true}]) {
        try {
          db[cn].ensureIndex(Object.assign({type: 'bitmap', fields: ['other']}, def));
          assertTrue(false, def);
        } catch (err) {
          assertEqual(require('internal').errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      }
    },

    testSingleLabel: function () {
      for (let l of allLabels) {
        check([l]);
      }
    },

    testSeveralLabels: function () {
      check(['a', 'b']);
      check(['c', 'a', 'h']);
      check(['a', 'b', 'c', 'd']);
      check(['a', 'a', 'b']);
    },

    testMissingLabel: function () {
      check(['nope']);
      check(['a', 'nope']);
    },

    testNumbersMatchByValue: function () {
      check([1]);
      check([1.0, 2]);
      check([2, 'a']);
    },

    testSingleValueIn: function () {
      const q = `FOR d IN ${cn} FILTER 'b' IN d.labels RETURN d._key`;
      assertEqual(1, bitmapIndexNodes(q, {}).length);
      assertEqual(expected(['b']), query(q, {}));
    },

    testEmptySearchDoesNotUseIndex: function () {
      const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels RETURN d._key`;
      assertEqual(0, bitmapIndexNodes(q, {labels: []}).length);
      assertEqual(Object.keys(docs).length, query(q, {labels: []}).length);
    },

    testUpdatesAndRemoves: function () {
      let c = db[cn];
      let random = makeRandom(42);
      let keys = Object.keys(docs);
      for (let i = 0; i < 500; ++i) {
        let key = keys[Math.floor(random() * keys.length)];
        if (docs[key] === undefined) {
          continue;
        }
        if (random() < 0.3) {
          c.remove(key);
          delete docs[key];
        } else {
          let labels = makeLabels(random);
          c.replace(key, labels === undefined ? {} : {labels});
          docs[key] = {_key: key};
          if (labels !== undefined) {
            docs[key].labels = labels;
          }
        }
      }
      check(['a']);
      check(['b', 'c']);
      check([1]);
    },

    testReadOwnWritesInStreamingTransaction: function () {
      const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels RETURN d._key`;
      let trx = db._createTransaction({collections: {write: [cn]}});
      try {
        let tc = trx.collection(cn);
        tc.insert({_key: 'new1', labels: ['zz', 'yy']});
        tc.insert({_key: 'new2', labels: ['zz']});
        let victim = expected(['a', 'b'])[0];
        tc.remove(victim);

        assertEqual(['new1', 'new2'], query(q, {labels: ['zz']}, trx));
        assertEqual(['new1'], query(q, {labels: ['zz', 'yy']}, trx));
        assertEqual(-1, query(q, {labels: ['a', 'b']}, trx).indexOf(victim));

        // not visible outside of the transaction
        assertEqual([], query(q, {labels: ['zz']}));
      } finally {
        trx.abort();
      }
      assertEqual([], query(q, {labels: ['zz']}));
      check(['a', 'b']);
    },

    testDeletesInStreamingTransaction: function () {
      const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels RETURN d._key`;
      let existing = expected(['a', 'b']);
      assertTrue(existing.length >= 2);
      let trx = db._createTransaction({collections: {write: [cn]}});
      try {
        let tc = trx.collection(cn);
        // inserted and removed again before any read
        tc.insert({_key: 'gone', labels: ['zz']});
        tc.remove('gone');
        // removed and inserted again under the same key with other labels
        tc.remove(existing[0]);
        tc.insert({_key: existing[0], labels: ['zz']});
        // removed, inserted and removed again
        tc.remove(existing[1]);
        tc.insert({_key: existing[1], labels: ['a', 'b']});
        tc.remove(existing[1]);
        trx.query(`INSERT {_key: 'gone2', labels: ['zz', 'a', 'b']} INTO ${cn}`);
        trx.query(`REMOVE 'gone2' IN ${cn}`);

        let stillThere = existing.slice(2);
        assertEqual([existing[0]], query(q, {labels: ['zz']}, trx));
        assertEqual(stillThere, query(q, {labels: ['a', 'b']}, trx));
        trx.commit();
        delete docs[existing[1]];
        docs[existing[0]] = {_key: existing[0], labels: ['zz']};
      } catch (err) {
        trx.abort();
        throw err;
      }
      assertEqual([existing[0]], query(q, {labels: ['zz']}));
      check(['a', 'b']);
      check(['a']);
    },

    testFailedOperationsInTransactionLeaveNoEntries: function () {
      // single shard, so that the unique index needs no shard key; it is
      // created after the bitmap index and fails after it was written
      const other = cn + 'Unique';
      db._drop(other);
      let c = db._create(other, {numberOfShards: 1});
      try {
        c.ensureIndex({type: 'bitmap', fields: ['labels'], values});
        c.ensureIndex({type: 'persistent', fields: ['u'], unique: true});
        c.insert({_key: 'keep', labels: ['zz'], u: 0});
        const q = `FOR d IN ${other} FILTER @labels ALL IN d.labels RETURN d._key`;
        const conflict = errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code;
        let trx = db._createTransaction({collections: {write: [other]}});
        try {
          let tc = trx.collection(other);
          tc.insert({_key: 'a', labels: ['zz', 'yy'], u: 1});
          for (let doc of [{_key: 'b', labels: ['yy', 'xx'], u: 1},
                           {_key: 'c', labels: ['xx'], u: 0}]) {
            try {
              tc.insert(doc);
              fail();
            } catch (err) {
              assertEqual(conflict, err.errorNum);
            }
          }
          try {
            tc.replace('a', {labels: ['xx'], u: 0});
            fail();
          } catch (err) {
            assertEqual(conflict, err.errorNum);
          }
          assertEqual(['a', 'keep'], query(q, {labels: ['zz']}, trx));
          assertEqual(['a'], query(q, {labels: ['yy']}, trx));
          assertEqual([], query(q, {labels: ['xx']}, trx));
          tc.update('keep', {labels: ['xx']});
          trx.commit();
        } catch (err) {
          trx.abort();
          throw err;
        }
        assertEqual(['a'], query(q, {labels: ['zz']}));
        assertEqual(['a'], query(q, {labels: ['yy']}));
        assertEqual(['keep'], query(q, {labels: ['xx']}));
      } finally {
        db._drop(other);
      }
    },

    testLargeTransactions: function () {
      const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels RETURN d._key`;
      const count = `FOR d IN ${cn} FILTER @labels ALL IN d.labels
                       COLLECT WITH COUNT INTO n RETURN n`;
      const insert = (label, from, to) => `FOR i IN ${from}..${to}
        INSERT {_key: CONCAT('${label}', i), labels: ['${label}', i % 2 == 0 ? 'even' : 'odd']}
        INTO ${cn}`;
      let trx = db._createTransaction({collections: {write: [cn]}});
      try {
        trx.query(insert('hot', 1, 20000));
        assertEqual([20000], trx.query(count, {labels: ['hot']}).toArray());
        trx.query(`FOR i IN 1..20000 FILTER i % 4 == 0 REMOVE CONCAT('hot', i) IN ${cn}`);
        assertEqual([15000], trx.query(count, {labels: ['hot']}).toArray());
        assertEqual([5000], trx.query(count, {labels: ['hot', 'even']}).toArray());
        assertEqual([0], db._query(count, {labels: ['hot']}).toArray());
        trx.commit();
      } catch (err) {
        trx.abort();
        throw err;
      }
      assertEqual([15000], db._query(count, {labels: ['hot']}).toArray());
      assertEqual([5000], db._query(count, {labels: ['hot', 'even']}).toArray());

      // intermediate commits write the pending entries as well
      db._query(insert('warm', 1, 20000), {}, {intermediateCommitCount: 777});
      assertEqual([20000], db._query(count, {labels: ['warm']}).toArray());
      assertEqual([10000], db._query(count, {labels: ['warm', 'odd']}).toArray());
      const fullScan = `FOR d IN ${cn} OPTIONS {disableIndex: true}
                          FILTER @labels ALL IN d.labels RETURN d._key`;
      assertEqual(query(fullScan, {labels: ['warm', 'odd']}),
                  query(q, {labels: ['warm', 'odd']}));
    },

    testTruncate: function () {
      db[cn].truncate({compact: false});
      docs = {};
      check(['a']);
      db[cn].insert({_key: 'again', labels: ['a', 'x']});
      docs.again = {_key: 'again', labels: ['a', 'x']};
      check(['a', 'x']);
    },

    testCoveredFilterIsRemoved: function () {
      for (let filter of [`@labels ALL IN d.labels`,
                          `'a' IN d.labels AND 'b' NOT IN d.labels`,
                          `@labels ANY IN d.labels`]) {
        const q = `FOR d IN ${cn} FILTER ${filter} RETURN d._key`;
        let bindVars = filter.includes('@labels') ? {labels: ['a', 'c']} : {};
        let plan = db._createStatement({query: q, bindVars}).explain().plan;
        let types = plan.nodes.map((n) => n.type);
        assertEqual(-1, types.indexOf('FilterNode'), filter);
        let indexNodes = plan.nodes.filter((n) => n.type === 'IndexNode');
        assertEqual(1, indexNodes.length, filter);
        assertEqual(undefined, indexNodes[0].filter, filter);
      }
    },

    testCountOnly: function () {
      const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels
                   COLLECT WITH COUNT INTO n RETURN n`;
      assertEqual(1, bitmapIndexNodes(q, {labels: ['a', 'b']}).length);
      assertEqual([expected(['a', 'b']).length], query(q, {labels: ['a', 'b']}));
    },

    testBooleanPredicates: function () {
      // [filter, whether the bitmap index answers it]
      const cases = [
        [`['a', 'h'] ANY IN d.labels`, true],
        [`[1, 'g'] ANY IN d.labels`, true],
        [`'a' IN d.labels AND 'b' NOT IN d.labels`, true],
        [`'a' IN d.labels AND ['b', 'c'] NONE IN d.labels`, true],
        [`'a' IN d.labels AND ['b', 'c'] ALL NOT IN d.labels`, true],
        [`'c' IN d.labels AND ['a', 'b'] ANY NOT IN d.labels`, true],
        [`['a', 'b'] NONE NOT IN d.labels`, true],
        [`'a' IN d.labels AND NOT ('b' IN d.labels)`, true],
        [`('a' IN d.labels AND 'b' IN d.labels) OR 'h' IN d.labels`, true],
        [`'a' IN d.labels OR 'b' IN d.labels`, true],
        [`['a', 'b'] ANY IN d.labels AND 'c' NOT IN d.labels AND 'd' IN d.labels`, true],
        [`'a' NOT IN d.labels`, false],
        [`['a', 'b'] NONE IN d.labels`, false],
        // the OR branches use different indexes
        [`'a' IN d.labels OR d._key == 'k1'`, true],
        [`'a' IN d.labels OR d.other == 1`, false],
        [`[] ANY IN d.labels`, false],
        [`['a', 'b'] AT LEAST (2) IN d.labels`, false],
      ];
      for (let [filter, usesIndex] of cases) {
        const q = `FOR d IN ${cn} FILTER ${filter} RETURN d._key`;
        const fullScan = `FOR d IN ${cn} OPTIONS {disableIndex: true} FILTER ${filter} RETURN d._key`;
        assertEqual(usesIndex, bitmapIndexNodes(q, {}).length > 0, filter);
        let actual = query(q, {});
        assertEqual(query(fullScan, {}), actual, filter);
      }
    },

    testRuntimeValues: function () {
      const rn = cn + 'Rules';
      db._drop(rn);
      let rules = db._create(rn);
      try {
        rules.insert([
          {_key: 'one', labels: ['a'], label: 'b'},
          {_key: 'two', labels: ['a', 'b'], label: 1},
          {_key: 'empty', labels: [], label: null},
          {_key: 'scalar', labels: 'a', label: ['a']},
          {_key: 'null', labels: null, label: {}},
          {_key: 'nested', labels: [['a'], 'b'], label: 'nope'},
          {_key: 'numbers', labels: [1, 2], label: 'h'},
          {_key: 'unknown', labels: ['nope'], label: 'c'},
        ]);
        // a value that is no array counts as an empty array, on both sides
        const elements = (v) => Array.isArray(v) ? v : [];
        const has = (labels, v) => elements(labels).some((e) => _.isEqual(e, v));
        const allIn = (r, d) => elements(r.labels).every((v) => has(d.labels, v));
        const anyIn = (r, d) => elements(r.labels).some((v) => has(d.labels, v));
        const cases = [
          [`r.labels ALL IN d.labels`, allIn],
          [`r.labels ANY IN d.labels`, anyIn],
          [`'a' IN d.labels AND r.labels NONE IN d.labels`,
           (r, d) => has(d.labels, 'a') && !anyIn(r, d)],
          [`r.labels ALL IN d.labels AND 'c' NOT IN d.labels`,
           (r, d) => allIn(r, d) && !has(d.labels, 'c')],
          [`r.label IN d.labels`, (r, d) => has(d.labels, r.label)],
          [`'a' IN d.labels AND r.label NOT IN d.labels`,
           (r, d) => has(d.labels, 'a') && !has(d.labels, r.label)],
        ];
        for (let [filter, matches] of cases) {
          const q = `FOR r IN ${rn} FOR d IN ${cn} FILTER ${filter} RETURN CONCAT(r._key, '/', d._key)`;
          let plan = db._createStatement({query: q}).explain().plan;
          let indexNodes = plan.nodes.filter((n) => n.type === 'IndexNode' &&
                                                  n.indexes.some((i) => i.type === 'bitmap'));
          assertEqual(1, indexNodes.length, filter);
          // the index answers the condition exactly
          assertEqual(undefined, indexNodes[0].filter, filter);
          assertFalse(plan.nodes.some((n) => n.type === 'FilterNode'), filter);

          let expected = [];
          for (let r of rules.toArray()) {
            for (let d of Object.values(docs)) {
              if (matches(r, d)) {
                expected.push(r._key + '/' + d._key);
              }
            }
          }
          assertEqual(expected.sort(), query(q, {}), filter);
        }
      } finally {
        db._drop(rn);
      }
    },

    testNestedValuesAndNumbers: function () {
      let c = db[cn];
      let extra = [
        {_key: 'nested1', labels: [['n', 1], {a: 1, b: 2}, 1]},
        {_key: 'nested2', labels: [['n', 1.0], {b: 2, a: 1.0}]},
        {_key: 'nested3', labels: [['n', 2], {a: 1}]},
      ];
      if (values === 'scalar') {
        // documents with values that cannot be stored are rejected
        for (let doc of extra) {
          try {
            c.insert(doc);
            fail();
          } catch (err) {
            assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum, doc);
          }
          assertFalse(c.exists(doc._key));
        }
        // ... so no document can contain such a value
        const q = `FOR d IN ${cn} FILTER [['n', 1]] ALL IN d.labels RETURN d._key`;
        assertEqual(1, bitmapIndexNodes(q, {}).length);
        assertEqual([], query(q, {}));
        const numbers = `FOR d IN ${cn} FILTER [1.0, 2] ALL IN d.labels RETURN d._key`;
        const fullScan = `FOR d IN ${cn} OPTIONS {disableIndex: true} FILTER [1.0, 2] ALL IN d.labels RETURN d._key`;
        assertTrue(query(numbers, {}).length > 0);
        assertEqual(query(fullScan, {}), query(numbers, {}));
        return;
      }
      c.insert(extra);
      const cases = [
        `[['n', 1]] ALL IN d.labels`,
        `[['n', 1.0], {a: 1, b: 2}] ALL IN d.labels`,
        `{b: 2.0, a: 1} IN d.labels`,
        `[['n', 2], {a: 1}] ANY IN d.labels`,
        `[1.0] ALL IN d.labels`,
        `['n', 2] IN d.labels AND {a: 1, b: 2} NOT IN d.labels`,
      ];
      for (let filter of cases) {
        const q = `FOR d IN ${cn} FILTER ${filter} RETURN d._key`;
        const fullScan = `FOR d IN ${cn} OPTIONS {disableIndex: true} FILTER ${filter} RETURN d._key`;
        assertEqual(1, bitmapIndexNodes(q, {}).length, filter);
        let expected = query(fullScan, {});
        assertTrue(expected.length > 0, filter);
        assertEqual(expected, query(q, {}), filter);
      }
    },

    testLateMaterialization: function () {
      // the index only produces document ids, which are looked up in batches
      const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels RETURN d`;
      let plan = db._createStatement({query: q, bindVars: {labels: ['a']}}).explain().plan;
      assertTrue(plan.nodes.some((n) => n.type === 'MaterializeNode'), plan.nodes.map((n) => n.type));
      assertTrue(plan.rules.includes('batch-materialize-documents'), plan.rules);
      let keys = db._query(q, {labels: ['a']}).toArray().map((d) => d._key).sort();
      assertEqual(expected(['a']), keys);
    },

    testValuesOption: function () {
      // the variant is part of the index identity
      let other = values === 'any' ? 'scalar' : 'any';
      let idx = db[cn].ensureIndex({type: 'bitmap', fields: ['labels'], values: other});
      assertEqual(other, idx.values);
      assertEqual(2, db[cn].indexes().filter((i) => i.type === 'bitmap').length);
      assertTrue(db[cn].dropIndex(idx));
      try {
        db[cn].ensureIndex({type: 'bitmap', fields: ['labels'], values: 'nope'});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      if (values === 'scalar') {
        // building the index over a document it cannot store fails
        db[cn].insert({_key: 'nested', other: [[1]]});
        try {
          db[cn].ensureIndex({type: 'bitmap', fields: ['other'], values});
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
        assertEqual(1, db[cn].indexes().filter((i) => i.type === 'bitmap').length);
      }
    },

    testDropIndex: function () {
      const q = `FOR d IN ${cn} FILTER @labels ALL IN d.labels RETURN d._key`;
      let idx = db[cn].indexes().filter((i) => i.type === 'bitmap')[0];
      assertNotEqual(undefined, idx);
      assertTrue(db[cn].dropIndex(idx));
      assertEqual(0, bitmapIndexNodes(q, {labels: ['a']}).length);
      assertEqual(expected(['a']), query(q, {labels: ['a']}));
    },
  };
}

// both variants run in this file, so their test names need to differ
function withSuffix(suite, suffix) {
  return Object.fromEntries(Object.entries(suite).map(
    ([name, fn]) => [name.startsWith('test') ? name + suffix : name, fn]));
}

function BitmapIndexAnyValuesSuite() {
  return withSuffix(BitmapIndexSuite('any'), 'AnyValues');
}

function BitmapIndexScalarValuesSuite() {
  return withSuffix(BitmapIndexSuite('scalar'), 'ScalarValues');
}

jsunity.run(BitmapIndexAnyValuesSuite);
jsunity.run(BitmapIndexScalarValuesSuite);
return jsunity.done();
