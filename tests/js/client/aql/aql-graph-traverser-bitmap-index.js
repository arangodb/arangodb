/* jshint esnext: true */

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
const {assertEqual, assertTrue} = jsunity.jsUnity.assertions;
const db = require('internal').db;
const _ = require('lodash');

const vn = 'UnitTestsBitmapVertices';
const en = 'UnitTestsBitmapEdges';
const allLabels = ['x', 'y', 'z', 'w'];
const numVertices = 100;

function makeRandom(seed) {
  let state = seed;
  return () => {
    state = (state * 1103515245 + 12345) % 2147483648;
    return state / 2147483648;
  };
}

function TraversalBitmapIndexSuite() {
  const everyEdge = 'p.edges[* RETURN @labels ALL IN CURRENT.labels] ALL == true';

  let traversal = (direction, filter, options) => {
    return `WITH ${vn} FOR v, e, p IN 1..3 ${direction} @start ${en}
              ${options || ''}
              ${filter ? 'FILTER ' + filter : ''}
              RETURN CONCAT_SEPARATOR(',', p.edges[*]._key)`;
  };

  let run = (query, bindVars) => {
    return db._query(query, bindVars).toArray().sort();
  };

  // all paths without the label filter, filtered on the client
  let reference = (direction, start, labels, edgeMatches) => {
    let q = `WITH ${vn} FOR v, e, p IN 1..3 ${direction} @start ${en}
               RETURN {key: CONCAT_SEPARATOR(',', p.edges[*]._key),
                       labels: p.edges[*].labels}`;
    let hasAll = (l) => Array.isArray(l) && labels.every((s) => l.includes(s));
    return db._query(q, {start}).toArray()
             .filter((path) => edgeMatches(path.labels, hasAll))
             .map((path) => path.key)
             .sort();
  };

  let traversalIndexes = (query, bindVars) => {
    let plan = db._createStatement({query, bindVars}).explain().plan;
    let nodes = plan.nodes.filter((n) => n.type === 'TraversalNode');
    assertEqual(1, nodes.length);
    return nodes[0].indexes;
  };

  let isBitmap = (idx, field) => idx.type === 'bitmap' &&
                                 JSON.stringify(idx.fields) === JSON.stringify([field, 'labels']);

  return {
    setUpAll: function () {
      db._drop(en);
      db._drop(vn);
      let vc = db._create(vn, {numberOfShards: 3});
      let ec = db._createEdgeCollection(en, {numberOfShards: 3});
      let vertices = [];
      for (let i = 0; i < numVertices; ++i) {
        vertices.push({_key: 'v' + i});
      }
      vc.insert(vertices);

      let random = makeRandom(1234);
      let edges = [];
      for (let i = 0; i < numVertices * 5; ++i) {
        let from = Math.floor(random() * numVertices);
        let to = Math.floor(random() * numVertices);
        let labels = allLabels.filter(() => random() < 0.6);
        edges.push({_key: 'e' + i, _from: vn + '/v' + from, _to: vn + '/v' + to, labels});
      }
      ec.insert(edges);
      ec.ensureIndex({type: 'bitmap', fields: ['_from', 'labels']});
      ec.ensureIndex({type: 'bitmap', fields: ['_to', 'labels']});
    },

    tearDownAll: function () {
      db._drop(en);
      db._drop(vn);
    },

    testExplainUsesBitmapIndexForEveryDirection: function () {
      for (let [direction, fields] of [['OUTBOUND', ['_from']],
                                       ['INBOUND', ['_to']],
                                       ['ANY', ['_from', '_to']]]) {
        let indexes = traversalIndexes(traversal(direction, everyEdge),
                                       {start: vn + '/v0', labels: ['x']});
        for (let field of fields) {
          assertTrue(indexes.base.some((idx) => isBitmap(idx, field)),
                     JSON.stringify({direction, indexes}));
        }
      }
    },

    testExplainWithoutFilterUsesEdgeIndex: function () {
      let indexes = traversalIndexes(traversal('OUTBOUND', null), {start: vn + '/v0'});
      assertTrue(indexes.base.every((idx) => idx.type === 'edge'), JSON.stringify(indexes));
    },

    testEveryEdgeMatchesReference: function () {
      let everyEdgeMatches = (labelsPerEdge, hasAll) => labelsPerEdge.every(hasAll);
      for (let direction of ['OUTBOUND', 'INBOUND', 'ANY']) {
        for (let labels of [['x'], ['x', 'y'], ['w', 'z', 'y']]) {
          for (let start of ['v0', 'v17', 'v42']) {
            let bindVars = {start: vn + '/' + start, labels};
            let expected = reference(direction, bindVars.start, labels, everyEdgeMatches);
            assertEqual(expected, run(traversal(direction, everyEdge), bindVars),
                        JSON.stringify({direction, labels, start}));
          }
        }
      }
    },

    testBooleanEdgeConditions: function () {
      const has = (l, x) => Array.isArray(l) && l.includes(x);
      // [projection evaluated per edge, the same on the client]
      const cases = [
        [`['x', 'w'] ANY IN CURRENT.labels`, (l) => has(l, 'x') || has(l, 'w')],
        [`'x' IN CURRENT.labels AND 'y' NOT IN CURRENT.labels`,
         (l) => has(l, 'x') && !has(l, 'y')],
        [`('x' IN CURRENT.labels OR 'w' IN CURRENT.labels) AND ['z'] NONE IN CURRENT.labels`,
         (l) => (has(l, 'x') || has(l, 'w')) && !has(l, 'z')],
        [`['x', 'y'] NONE NOT IN CURRENT.labels`, (l) => has(l, 'x') && has(l, 'y')],
      ];
      for (let [projection, edgeMatches] of cases) {
        for (let direction of ['OUTBOUND', 'ANY']) {
          let filter = `p.edges[* RETURN ${projection}] ALL == true`;
          let bindVars = {start: vn + '/v7'};
          let indexes = traversalIndexes(traversal(direction, filter), bindVars);
          assertTrue(indexes.base.some((idx) => isBitmap(idx, '_from')),
                     JSON.stringify({projection, indexes}));
          let expected = reference(direction, bindVars.start, [],
                                   (labelsPerEdge) => labelsPerEdge.every(edgeMatches));
          assertEqual(expected, run(traversal(direction, filter), bindVars),
                      JSON.stringify({projection, direction}));
        }
      }
    },

    testRuntimeLabels: function () {
      const rn = 'UnitTestsBitmapRules';
      db._drop(rn);
      let rules = db._create(rn);
      try {
        rules.insert([
          {_key: 'x', labels: ['x']},
          {_key: 'xy', labels: ['x', 'y']},
          {_key: 'empty', labels: []},
          {_key: 'scalar', labels: 'x'},
          {_key: 'nested', labels: [['x'], 'y']},
        ]);
        const start = vn + '/v11';
        const q = `WITH ${vn} FOR r IN ${rn}
                     FOR v, e, p IN 1..2 OUTBOUND @start ${en}
                       FILTER p.edges[* RETURN r.labels ALL IN CURRENT.labels] ALL == true
                       RETURN CONCAT(r._key, ':', CONCAT_SEPARATOR(',', p.edges[*]._key))`;
        let plan = db._createStatement({query: q, bindVars: {start}}).explain().plan;
        let tn = plan.nodes.filter((n) => n.type === 'TraversalNode')[0];
        assertTrue(tn.indexes.base.some((idx) => isBitmap(idx, '_from')),
                   JSON.stringify(tn.indexes));

        let paths = db._query(`WITH ${vn} FOR v, e, p IN 1..2 OUTBOUND @start ${en}
                                 RETURN {key: CONCAT_SEPARATOR(',', p.edges[*]._key),
                                         labels: p.edges[*].labels}`, {start}).toArray();
        let expected = [];
        for (let r of rules.toArray()) {
          // a value that is no array counts as an empty array, so the
          // scalar and the empty rule match every edge
          let searched = Array.isArray(r.labels) ? r.labels : [];
          let edgeMatches = (l) => searched.every((s) => l.some((e) => _.isEqual(e, s)));
          for (let path of paths) {
            if (path.labels.every(edgeMatches)) {
              expected.push(r._key + ':' + path.key);
            }
          }
        }
        assertEqual(expected.sort(), run(q, {start}));
      } finally {
        db._drop(rn);
      }
    },

    testDepthSpecificCondition: function () {
      let filter = '@labels ALL IN p.edges[0].labels';
      let firstEdgeMatches = (labelsPerEdge, hasAll) => hasAll(labelsPerEdge[0]);
      let bindVars = {start: vn + '/v3', labels: ['y', 'z']};
      let indexes = traversalIndexes(traversal('OUTBOUND', filter), bindVars);
      assertTrue(indexes.levels['0'].some((idx) => isBitmap(idx, '_from')),
                 JSON.stringify(indexes));
      assertEqual(reference('OUTBOUND', bindVars.start, bindVars.labels, firstEdgeMatches),
                  run(traversal('OUTBOUND', filter), bindVars));
    },

    testUnknownLabelFindsNothing: function () {
      let bindVars = {start: vn + '/v0', labels: ['x', 'unknown']};
      assertEqual([], run(traversal('ANY', everyEdge), bindVars));
    },

    testUpdatesAreVisible: function () {
      let first = db._query(`FOR e IN ${en} FILTER e._from != e._to LIMIT 1 RETURN e`)
                    .toArray()[0];
      let bindVars = {start: first._from, labels: ['marker']};
      assertEqual([], run(traversal('OUTBOUND', everyEdge), bindVars));
      db[en].update(first._key, {labels: first.labels.concat(['marker'])});
      try {
        assertEqual([first._key], run(traversal('OUTBOUND', everyEdge), bindVars));
      } finally {
        db[en].update(first._key, {labels: first.labels}, {mergeObjects: false});
      }
      assertEqual([], run(traversal('OUTBOUND', everyEdge), bindVars));
    },
  };
}

jsunity.run(TraversalBitmapIndexSuite);
return jsunity.done();
