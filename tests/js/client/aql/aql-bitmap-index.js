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
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const db = require('internal').db;

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

function BitmapIndexSuite() {
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
      let idx = c.ensureIndex({type: 'bitmap', fields: ['labels']});
      assertEqual('bitmap', idx.type);
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

jsunity.run(BitmapIndexSuite);
return jsunity.done();
