/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

// Needs an arangod built with USE_MEMORY_TRACKING: without the malloc
// override the new monitor counts nothing and its limit never fires.

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");

function newMonitorMemoryLimitSuite() {
  'use strict';
  // memoryLimit drives only the new monitor (temporary, see Query.cpp).
  const query = "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)";

  return {
    testQueryOverNewMonitorLimitFailsWithResourceLimit: function () {
      // Without the limit the same query succeeds.
      assertEqual(100000, db._query(query, null, {memoryLimit: 0}).toArray().length);

      try {
        db._query(query, null, {memoryLimit: 64 * 1024}).toArray();
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, err.errorNum, err.errorMessage);
      }
    },

    testLimitSweepFailsCleanlyUntilQuerySucceeds: function () {
      // Deterministic: no collection, no randomness.
      const sweepQuery = "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)";
      const expected = db._query(sweepQuery, null, {memoryLimit: 0}).toArray();
      assertEqual(10000, expected.length);

      // Each step lets the query get further, so the limit hits a different allocation site.
      const step = 4 * 1024;
      const maxLimit = 8 * 1024 * 1024;
      let failures = 0;
      let result = null;
      for (let limit = step; limit <= maxLimit && result === null; limit += step) {
        let error = null;
        try {
          result = db._query(sweepQuery, null, {memoryLimit: limit}).toArray();
        } catch (err) {
          error = err;
        }
        if (error !== null) {
          assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, error.errorNum,
                      `limit ${limit}: ${error.errorMessage}`);
          ++failures;
        }
      }
      assertTrue(failures > 0, "a 4 KB limit should already fail");
      assertEqual(expected, result, "no limit up to 8 MB was enough");
    },
  };
}

// Raises the limit by `growth` from 4 KB until the query succeeds: every attempt
// below must fail with a resource limit error, and the success must match an
// unlimited run.
// `unordered`: compare as multisets, for queries whose row order is not defined.
// `beforeEach`: runs before every attempt, e.g. to clear a cache the query fills.
function assertLimitSweep(query, {bindVars = null, unordered = false,
                                  queryOptions = {}, beforeEach = () => {}} = {}) {
  const run = (limit) => {
    beforeEach();
    const rows = db._query(query, bindVars, {...queryOptions, memoryLimit: limit}).toArray();
    return unordered ? rows.map((r) => JSON.stringify(r)).sort() : rows;
  };
  const expected = run(0);
  const growth = 1.25;
  const maxLimit = 1024 * 1024 * 1024;
  let failures = 0;
  let result = null;
  for (let limit = 4 * 1024; limit <= maxLimit && result === null;
       limit = Math.ceil(limit * growth)) {
    try {
      result = run(limit);
    } catch (err) {
      assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, err.errorNum,
                  `limit ${limit}: ${err.errorMessage}`);
      ++failures;
    }
  }
  assertTrue(failures > 0, "a 4 KB limit should already fail");
  assertEqual(expected, result, `no limit up to ${maxLimit} was enough`);
}

// Queries without collections (section A of the memory accounting report).
function newMonitorLimitPureAqlSuite() {
  'use strict';
  const queries = {
    Return1: "RETURN 1",
    LoopWithEmptyResult: "FOR i IN 1..1000000 FILTER i == -1 RETURN i",
    LoopReturning1MRows: "FOR i IN 1..1000000 RETURN i",
    ConcatStrings: "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)",
    ArrayOf100kObjects: "RETURN LENGTH(FOR i IN 1..100000 RETURN {[CONCAT('k', i)]: i})",
    MergeInto100kKeyObject: "RETURN LENGTH(MERGE(FOR i IN 1..100000 RETURN {[CONCAT('k', i)]: i}))",
    ManySmallMerges: "FOR i IN 1..100000 LET m = MERGE({a: i}, {b: i}) FILTER m.a == -1 RETURN m",
    ConcatSeparator2MBString: "LET s = CONCAT_SEPARATOR('', (FOR i IN 1..200000 RETURN 'abcdefghij')) RETURN LENGTH(s)",
    Unique100kValues: "RETURN LENGTH(UNIQUE(FOR i IN 1..100000 RETURN i % 50000))",
    Sorted100kValues: "RETURN LENGTH(SORTED(FOR i IN 1..100000 RETURN (i * 7919) % 100003))",
  };

  const suite = {};
  for (const [name, query] of Object.entries(queries)) {
    suite[`testLimitSweep${name}`] = () => assertLimitSweep(query);
  }
  return suite;
}

// Reads, aggregations and lookups on plain collections (sections A and B).
function newMonitorLimitDocumentsSuite() {
  'use strict';
  const t = "UnitTestsMemLimitT";    // 100k docs {v}
  const r = "UnitTestsMemLimitR";    // 10k docs, keys k1..k10000 {v}
  const qc = "UnitTestsMemLimitQc";  // 10k docs {v, s}, persistent index on v
  const gv = "UnitTestsMemLimitGv";  // 10k docs {v}, persistent index on v
  const keys = (n) => Array.from({length: n}, (_, i) => `k${i + 1}`);
  const inKeys = `FOR d IN ${r} FILTER d._key IN @keys COLLECT AGGREGATE s = SUM(d.v) RETURN s`;

  const queries = {
    RegexFilter: [`FOR d IN ${qc} FILTER REGEX_TEST(d.s, 'foobarbaz[0-9]+5$') COLLECT WITH COUNT INTO n RETURN n`],
    CollectHash: [`FOR d IN ${qc} COLLECT g = d.v % 100 OPTIONS {method: 'hash'} RETURN g`, {unordered: true}],
    CollectSorted: [`FOR d IN ${qc} COLLECT g = d.v % 100 OPTIONS {method: 'sorted'} RETURN g`],
    CollectInto: [`FOR d IN ${qc} COLLECT g = d.v % 100 INTO grp RETURN LENGTH(grp)`, {unordered: true}], // optimizer (resolved)
    CollectAggregate: [`FOR d IN ${qc} COLLECT g = d.v % 100 AGGREGATE s = SUM(d.v), m = MAX(d.s) RETURN [g, s, m]`, {unordered: true}], // plan clone
    ReturnDistinct: [`FOR d IN ${qc} RETURN DISTINCT d.v % 100`, {unordered: true}],
    CollectCount: [`FOR d IN ${qc} COLLECT WITH COUNT INTO n RETURN n`],
    WindowRows: [`FOR d IN ${qc} SORT d.v WINDOW {preceding: 10, following: 10} AGGREGATE s = SUM(d.v) RETURN s`],
    WindowRange: [`FOR d IN ${qc} SORT d.v WINDOW d.v WITH {preceding: 100} AGGREGATE s = SUM(d.v) RETURN s`],
    WindowUnbounded: [`FOR d IN ${qc} SORT d.v WINDOW {preceding: 'unbounded'} AGGREGATE s = SUM(d.v) RETURN s`],
    CorrelatedSubquery: [`FOR a IN ${qc} LET s = (FOR b IN ${gv} FILTER b.v == a.v RETURN b._key) COLLECT WITH COUNT INTO n RETURN n`],
    SubqueryHolding10kDocs: [`LET docs = (FOR b IN ${gv} RETURN b) FOR a IN ${qc} LIMIT 100 RETURN docs[a.v - 1].v`, {unordered: true}],
    FullScanCount: [`FOR d IN ${t} FILTER d.v > 0 COLLECT WITH COUNT INTO n RETURN n`],
    ReturnWholeDocuments: [`FOR d IN ${r} FILTER d.v <= 1000 RETURN d`, {unordered: true}],
    ReturnDocumentLengths: [`FOR d IN ${r} FILTER d.v <= 1000 RETURN LENGTH(d)`],
    Sort100kValues: [`FOR d IN ${t} SORT d.v DESC RETURN d.v`],
    DocumentLookups: [`FOR i IN 1..1000 LET d = DOCUMENT(${r}, CONCAT('k', i)) COLLECT AGGREGATE s = SUM(d.v) RETURN s`], // noexcept
    InList100Keys: [inKeys, {bindVars: {keys: keys(100)}}], // noexcept
    InList1000Keys: [inKeys, {bindVars: {keys: keys(1000)}}], // noexcept
    InList10000Keys: [inKeys, {bindVars: {keys: keys(10000)}}], // noexcept
    PersistentIndexLookups: [`FOR i IN 1..1000 FOR d IN ${qc} FILTER d.v == i RETURN d.s`],
    IndexJoin: [`FOR a IN ${qc} FOR b IN ${gv} FILTER b.v == a.v COLLECT WITH COUNT INTO n RETURN n`],
  };

  const suite = {
    setUpAll: function () {
      [t, r, qc, gv].forEach((c) => db._drop(c));
      db._create(t);
      db._query(`FOR i IN 1..100000 INSERT {v: i} INTO ${t}`);
      db._create(r);
      db._query(`FOR i IN 1..10000 INSERT {_key: CONCAT('k', i), v: i} INTO ${r}`);
      db._create(qc);
      db._query(`FOR i IN 1..10000 INSERT {v: i, s: CONCAT('foobarbaz', i)} INTO ${qc}`);
      db[qc].ensureIndex({type: 'persistent', fields: ['v']});
      db._create(gv);
      db._query(`FOR i IN 1..10000 INSERT {v: i} INTO ${gv}`);
      db[gv].ensureIndex({type: 'persistent', fields: ['v']});
    },
    tearDownAll: function () {
      [t, r, qc, gv].forEach((c) => db._drop(c));
    },
  };
  for (const [name, [query, options]] of Object.entries(queries)) {
    suite[`testLimitSweep${name}`] = () => assertLimitSweep(query, options);
  }
  return suite;
}

// Geo, inverted index and ArangoSearch view (section B).
function newMonitorLimitGeoSearchSuite() {
  'use strict';
  const geo = "UnitTestsMemLimitGeo";  // 10k points, geo index on loc
  const qc = "UnitTestsMemLimitQc";    // 10k docs {v, s}, inverted index 'inv'
  const sv = "UnitTestsMemLimitSv";    // 10k docs {v, s}, linked to the view
  const view = "UnitTestsMemLimitView";

  const queries = {
    // Ties in distance can make LIMIT pick different rows; compare as a set.
    GeoNear: [`FOR d IN ${geo} SORT DISTANCE(d.loc[0], d.loc[1], 50, 10) LIMIT 100 RETURN d.v`, {unordered: true}],
    GeoRange: [`FOR d IN ${geo} FILTER DISTANCE(d.loc[0], d.loc[1], 50, 10) < 2000000 COLLECT WITH COUNT INTO n RETURN n`],
    InvertedIndex: [`FOR d IN ${qc} OPTIONS {indexHint: 'inv', forceIndexHint: true} FILTER d.v > 5000 COLLECT WITH COUNT INTO n RETURN n`],
    ArangoSearchView: [`FOR d IN ${view} SEARCH ANALYZER(d.s == 'foobarbaz', 'text_en') COLLECT WITH COUNT INTO n RETURN n`],
  };

  const suite = {
    setUpAll: function () {
      tearDownGeoSearch();
      db._create(geo);
      db._query(`FOR i IN 1..10000
                 INSERT {loc: [((i * 37) % 180) - 90, ((i * 101) % 360) - 180], v: i} INTO ${geo}`);
      db[geo].ensureIndex({type: 'geo', fields: ['loc'], geoJson: false});
      db._create(qc);
      db._query(`FOR i IN 1..10000 INSERT {v: i, s: CONCAT('foobarbaz', i)} INTO ${qc}`);
      db[qc].ensureIndex({type: 'inverted', name: 'inv', fields: ['s', 'v']});
      db._create(sv);
      db._query(`FOR i IN 1..10000 INSERT {v: i, s: CONCAT('foobarbaz ', i)} INTO ${sv}`);
      db._createView(view, 'arangosearch',
                     {links: {[sv]: {fields: {v: {}, s: {analyzers: ['text_en']}}}}});
      // Both are filled asynchronously; wait so every run sees the same data.
      db._query(`FOR d IN ${view} SEARCH d.v > 0 OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO n RETURN n`);
      db._query(`FOR d IN ${qc} OPTIONS {indexHint: 'inv', forceIndexHint: true, waitForSync: true}
                 FILTER d.v > 0 COLLECT WITH COUNT INTO n RETURN n`);
    },
    tearDownAll: function () {
      tearDownGeoSearch();
    },
  };
  function tearDownGeoSearch() {
    if (db._view(view)) {
      db._dropView(view);
    }
    [geo, qc, sv].forEach((c) => db._drop(c));
  }
  for (const [name, [query, options]] of Object.entries(queries)) {
    suite[`testLimitSweep${name}`] = () => assertLimitSweep(query, options);
  }
  return suite;
}

// Index, document, results and plan caches (section C).
function newMonitorLimitCachesSuite() {
  'use strict';
  const gv = "UnitTestsMemLimitGv";      // 10k vertices, keys v1..v10000
  const ge = "UnitTestsMemLimitGe";      // ~25k edges (edge index cache)
  const cdoc = "UnitTestsMemLimitCdoc";  // 1k docs, keys k1..k1000, cacheEnabled
  const cidx = "UnitTestsMemLimitCidx";  // 1k docs {v, u}, cached persistent indexes
  const qc = "UnitTestsMemLimitQc";      // 10k docs {v, s}, persistent index on v
  const resultsCache = require('@arangodb/aql/cache');
  const planCache = require('@arangodb/aql/plan-cache');
  let oldResultsCacheMode;

  const queries = {
    EdgeCacheLookups: [`FOR i IN 1..1000 FOR x IN ${ge} FILTER x._from == CONCAT('${gv}/v', i) COLLECT WITH COUNT INTO n RETURN n`],
    PrimaryIndexCacheLookups: [`FOR i IN 1..1000 FOR d IN ${cdoc} FILTER d._key == CONCAT('k', i) COLLECT AGGREGATE s = SUM(d.v) RETURN s`],
    PersistentIndexCacheLookups: [`FOR i IN 1..1000 FOR d IN ${cidx} FILTER d.v == i COLLECT AGGREGATE s = SUM(d.v) RETURN s`], // error code (resolved)
    UniqueIndexCacheLookups: [`FOR i IN 1..1000 FOR d IN ${cidx} FILTER d.u == i COLLECT AGGREGATE s = SUM(d.u) RETURN s`], // error code (resolved)
    DocumentCacheLookups: [`FOR i IN 1..1000 LET d = DOCUMENT(${cdoc}, CONCAT('k', i)) COLLECT AGGREGATE s = SUM(d.v) RETURN s`], // noexcept
    // Cleared before every attempt, otherwise all attempts after the first are cache hits.
    ResultsCache: [`FOR d IN ${qc} RETURN d.s`,
                   {queryOptions: {cache: true}, beforeEach: () => resultsCache.clear()}],
    PlanCache: [`FOR d IN ${qc} FILTER d.v == @v RETURN d.s`,
                {bindVars: {v: 1}, queryOptions: {usePlanCache: true}, beforeEach: () => planCache.clear()}],
  };

  const suite = {
    setUpAll: function () {
      [gv, ge, cdoc, cidx, qc].forEach((c) => db._drop(c));
      db._create(gv);
      db._query(`FOR i IN 1..10000 INSERT {_key: CONCAT('v', i), v: i} INTO ${gv}`);
      db._createEdgeCollection(ge);
      db._query(`FOR i IN 1..10000 FOR t IN [i + 1, i * 2, i * 3] FILTER t <= 10000
                 INSERT {_from: CONCAT('${gv}/v', i), _to: CONCAT('${gv}/v', t)} INTO ${ge}`);
      db._create(cdoc, {cacheEnabled: true});
      db._query(`FOR i IN 1..1000 INSERT {_key: CONCAT('k', i), v: i} INTO ${cdoc}`);
      db._create(cidx);
      db._query(`FOR i IN 1..1000 INSERT {v: i, u: i} INTO ${cidx}`);
      db[cidx].ensureIndex({type: 'persistent', fields: ['v'], cacheEnabled: true});
      db[cidx].ensureIndex({type: 'persistent', fields: ['u'], unique: true, cacheEnabled: true});
      db._create(qc);
      db._query(`FOR i IN 1..10000 INSERT {v: i, s: CONCAT('foobarbaz', i)} INTO ${qc}`);
      db[qc].ensureIndex({type: 'persistent', fields: ['v']});
      oldResultsCacheMode = resultsCache.properties().mode;
      resultsCache.properties({mode: 'demand'});
    },
    tearDownAll: function () {
      resultsCache.properties({mode: oldResultsCacheMode});
      resultsCache.clear();
      planCache.clear();
      [gv, ge, cdoc, cidx, qc].forEach((c) => db._drop(c));
    },
  };
  for (const [name, [query, options]] of Object.entries(queries)) {
    suite[`testLimitSweep${name}`] = () => assertLimitSweep(query, options);
  }
  return suite;
}

// Like assertLimitSweep, for writes: `reset` restores the start state before
// every attempt, a failed attempt must leave `state()` unchanged (rolled back),
// and the first success must leave the same state as an unlimited run.
function assertWriteLimitSweep({reset, run, state}) {
  reset();
  run(0);
  const expected = state();
  const growth = 1.25;
  const maxLimit = 1024 * 1024 * 1024;
  let failures = 0;
  let succeeded = false;
  for (let limit = 4 * 1024; limit <= maxLimit && !succeeded;
       limit = Math.ceil(limit * growth)) {
    reset();
    const before = state();
    try {
      run(limit);
      succeeded = true;
    } catch (err) {
      assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, err.errorNum,
                  `limit ${limit}: ${err.errorMessage}`);
      assertEqual(before, state(), `limit ${limit}: failed write was not rolled back`);
      ++failures;
    }
  }
  assertTrue(failures > 0, "a 4 KB limit should already fail");
  assertTrue(succeeded, `no limit up to ${maxLimit} was enough`);
  assertEqual(expected, state());
}

// INSERT, UPDATE, REPLACE, UPSERT, REMOVE and stream transactions (section D).
function newMonitorLimitWritesSuite() {
  'use strict';
  const c = "UnitTestsMemLimitW";
  const recreate = () => {
    db._drop(c);
    db._create(c);
  };
  const with1000Docs = () => {
    recreate();
    db._query(`FOR i IN 1..1000 INSERT {_key: CONCAT('k', i), v: i, s: CONCAT('foobarbaz', i)} INTO ${c}`);
  };
  // Content summary; independent of generated keys and revisions.
  const state = () => db._query(
    `RETURN {n: LENGTH(${c}), v: SUM(FOR d IN ${c} RETURN d.v), u: SUM(FOR d IN ${c} RETURN d.u),
             r: SUM(FOR d IN ${c} RETURN d.r), a: SUM(FOR d IN ${c} RETURN LENGTH(d.a))}`).toArray()[0];
  const aql = (query) => (limit) => db._query(query, null, {memoryLimit: limit});

  const insertN = (n) => ({reset: recreate, run: aql(`FOR i IN 1..${n} INSERT {v: i} INTO ${c}`)});
  const cases = {};
  cases.Insert10 = insertN(10);
  cases.Insert100 = insertN(100);
  cases.Insert1000 = insertN(1000); // absl node_hash_map (resolved by patch)
  cases.Insert10000 = insertN(10000); // absl node_hash_map (resolved by patch)
  cases.Insert100000 = insertN(100000); // absl node_hash_map (resolved by patch)
  for (const n of [100, 1000, 10000]) {
    cases[`Insert1000WithRange${n}`] =
      {reset: recreate, run: aql(`FOR i IN 1..1000 INSERT {v: i, a: RANGE(1, ${n})} INTO ${c}`)};
  }
  cases.InsertExclusive =
    {reset: recreate, run: aql(`FOR i IN 1..1000 INSERT {v: i} INTO ${c} OPTIONS {exclusive: true}`)};
  cases.Update = {reset: with1000Docs, run: aql(`FOR d IN ${c} UPDATE d WITH {v: d.v + 1} IN ${c}`)};
  cases.Replace = {reset: with1000Docs, run: aql(`FOR d IN ${c} REPLACE d WITH {v: d.v, s: d.s, r: 1} IN ${c}`)}; // absl node_hash_map (resolved by patch)
  cases.Upsert = {reset: with1000Docs, run: aql( // noexcept
   `FOR i IN 1..1000 UPSERT {_key: CONCAT('k', i)} INSERT {_key: CONCAT('k', i), v: i} UPDATE {u: 1} IN ${c}`)};
  cases.Remove = {reset: with1000Docs, run: aql(`FOR d IN ${c} REMOVE d IN ${c}`)}; // absl node_hash_map (resolved by patch)
  // Stream transactions: commit on success, abort on failure.
  cases.StreamTrxInsert = {reset: recreate, run: (limit) => {
    const trx = db._createTransaction({collections: {write: [c]}});
    try {
      trx.query(`FOR i IN 1..1000 INSERT {v: i, s: CONCAT('foobarbaz', i)} INTO ${c}`, {}, {memoryLimit: limit});
    } catch (err) {
      trx.abort();
      throw err;
    }
    trx.commit();
  }};
  // Reads its own uncommitted writes; always aborted, so only the read is limited.
  cases.StreamTrxReadOwnWrites = {reset: recreate, run: (limit) => {
    const trx = db._createTransaction({collections: {write: [c]}});
    try {
      trx.query(`FOR i IN 1..1000 INSERT {v: i} INTO ${c}`);
      const n = trx.query(`FOR d IN ${c} COLLECT WITH COUNT INTO n RETURN n`, {}, {memoryLimit: limit}).toArray();
      assertEqual([1000], n);
    } finally {
      trx.abort();
    }
  }};

  const suite = {
    tearDownAll: function () {
      db._drop(c);
    },
  };
  for (const [name, testCase] of Object.entries(cases)) {
    suite[`testLimitSweep${name}`] = () => assertWriteLimitSweep({...testCase, state});
  }
  return suite;
}

// Traversals, path searches and MATCH (section E).
function newMonitorLimitGraphSuite() {
  'use strict';
  const gv = "UnitTestsMemLimitGv";  // 10k vertices, keys v1..v10000
  const ge = "UnitTestsMemLimitGe";  // ~25k edges i -> i+1, 2i, 3i, weight w
  const from = `FOR s IN 1..100 FOR v IN 1..4 OUTBOUND CONCAT('${gv}/v', s) ${ge}`;
  const sp = `FOR s IN 1..100 FOR v IN OUTBOUND SHORTEST_PATH CONCAT('${gv}/v', s) TO '${gv}/v10000' ${ge}`;
  const match = {queryOptions: {matchStatement: 'experimental'}};

  const queries = {
    TraversalBfs: [`${from} OPTIONS {order: 'bfs'} COLLECT WITH COUNT INTO n RETURN n`], // error code (resolved)
    TraversalDfs: [`${from} COLLECT WITH COUNT INTO n RETURN n`],
    TraversalBfsGlobalUniqueness: [`${from} OPTIONS {order: 'bfs', uniqueVertices: 'global'} COLLECT WITH COUNT INTO n RETURN n`], // error code (resolved)
    TraversalWeighted: [`FOR s IN 1..100 FOR v, e, p IN 1..4 OUTBOUND CONCAT('${gv}/v', s) ${ge} OPTIONS {order: 'weighted', weightAttribute: 'w'} COLLECT WITH COUNT INTO n RETURN n`], // error code (resolved)
    ShortestPath: [`${sp} COLLECT WITH COUNT INTO n RETURN n`],
    WeightedShortestPath: [`${sp} OPTIONS {weightAttribute: 'w'} COLLECT WITH COUNT INTO n RETURN n`],
    KShortestPaths: [`FOR p IN OUTBOUND K_SHORTEST_PATHS '${gv}/v1' TO '${gv}/v1000' ${ge} LIMIT 10 COLLECT WITH COUNT INTO n RETURN n`],
    KPaths: [`FOR p IN 1..8 OUTBOUND K_PATHS '${gv}/v1' TO '${gv}/v100' ${ge} COLLECT WITH COUNT INTO n RETURN n`],
    AllShortestPaths: [`FOR p IN OUTBOUND ALL_SHORTEST_PATHS '${gv}/v1' TO '${gv}/v1000' ${ge} COLLECT WITH COUNT INTO n RETURN n`],
    MatchOneHop: [`MATCH (v :${gv}) -[ e:${ge} ]-> (w :${gv}) COLLECT WITH COUNT INTO n RETURN n`, match], // error code (resolved), optimizer (resolved)
    MatchOneToThreeHops: [`MATCH (v :${gv} WHERE v.v <= 100) -[ e:${ge} * 1..3 ]-> (w :${gv}) COLLECT WITH COUNT INTO n RETURN n`, match],
    MatchBoundEnds: [`WITH ${gv} FOR v IN ['${gv}/v1'] FOR w IN ${gv} FILTER w.v <= 200 MATCH (v) -[ e:${ge} * 1..4 ]-> (w) COLLECT WITH COUNT INTO n RETURN n`, match], // error code (resolved)
  };

  const suite = {
    setUpAll: function () {
      [gv, ge].forEach((c) => db._drop(c));
      db._create(gv);
      db._query(`FOR i IN 1..10000 INSERT {_key: CONCAT('v', i), v: i} INTO ${gv}`);
      db._createEdgeCollection(ge);
      db._query(`FOR i IN 1..10000 FOR t IN [i + 1, i * 2, i * 3] FILTER t <= 10000
                 INSERT {_from: CONCAT('${gv}/v', i), _to: CONCAT('${gv}/v', t), w: t % 7 + 1} INTO ${ge}`);
      db[gv].ensureIndex({type: 'persistent', fields: ['v']});
    },
    tearDownAll: function () {
      [gv, ge].forEach((c) => db._drop(c));
    },
  };
  for (const [name, [query, options]] of Object.entries(queries)) {
    suite[`testLimitSweep${name}`] = () => assertLimitSweep(query, options);
  }
  return suite;
}

// Vector search, sort in memory and spilled to disk, cursor in batches (section F).
function newMonitorLimitSpecialSuite() {
  'use strict';
  const vec = "UnitTestsMemLimitVec";  // 10k 128-dimension vectors, vector index
  const qc = "UnitTestsMemLimitQc";    // 10k docs {v, s}
  const queryVector = Array.from({length: 128}, (_, j) => (j % 16) / 16);
  const sortQuery = `FOR d IN ${qc} SORT d.s DESC RETURN d.v`;

  const queries = {
    // Approximate search; compare the 10 keys as a set.
    VectorSearch: [`FOR d IN ${vec} SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 10 RETURN d._key`,
                   {bindVars: {qp: queryVector}, unordered: true}],
    SortInMemory: [sortQuery],
    SortSpilledToDisk: [sortQuery, {queryOptions: {spillOverThresholdNumRows: 1000}}],
    // F3 kept a cursor open until its timeout; here the batches are read to the end.
    CursorInBatches: [`FOR d IN ${qc} RETURN d`, {queryOptions: {batchSize: 10}, unordered: true}],
  };

  const suite = {
    setUpAll: function () {
      [vec, qc].forEach((c) => db._drop(c));
      db._create(vec);
      db._query(`FOR i IN 1..10000
                 INSERT {_key: CONCAT('k', i), vector: (FOR j IN 1..128 RETURN ((i * j) % 97) / 97)} INTO ${vec}`);
      db[vec].ensureIndex({type: 'vector', fields: ['vector'],
                           params: {metric: 'l2', dimension: 128, nLists: 10}});
      db._create(qc);
      db._query(`FOR i IN 1..10000 INSERT {v: i, s: CONCAT('foobarbaz', i)} INTO ${qc}`);
    },
    tearDownAll: function () {
      [vec, qc].forEach((c) => db._drop(c));
    },
  };
  for (const [name, [query, options]] of Object.entries(queries)) {
    suite[`testLimitSweep${name}`] = () => assertLimitSweep(query, options);
  }
  return suite;
}

// jsunity.run(newMonitorMemoryLimitSuite);
// jsunity.run(newMonitorLimitPureAqlSuite);
 jsunity.run(newMonitorLimitDocumentsSuite);
// jsunity.run(newMonitorLimitGeoSearchSuite);
//jsunity.run(newMonitorLimitCachesSuite);
// jsunity.run(newMonitorLimitWritesSuite);
//jsunity.run(newMonitorLimitGraphSuite); // all good
// jsunity.run(newMonitorLimitSpecialSuite);

return jsunity.done();
