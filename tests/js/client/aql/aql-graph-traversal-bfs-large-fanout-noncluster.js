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
// //////////////////////////////////////////////////////////////////////////////

'use strict';

// Regression test for issue #22821.
//
// 3.12.8 reworked single-server traversals to fetch neighbours in batches via
// "neighbour cursors". Both the BFS queue (CursorFifoQueue) and the DFS queue
// (CursorLifoQueue) implement pop() as tail recursion that the compiler does
// not eliminate, so every continuation cursor that yields no further steps
// costs one stack frame instead of one loop iteration. A long enough run of
// contiguous non-producing cursors overflows the scheduler thread stack and
// crashes the server (it surfaced as a misleading null-deref deep in RocksDB).
//
// OneSidedEnumerator::computeNeighbourhoodOfNextVertex appends a neighbour
// cursor for every expandable vertex before knowing whether it has outgoing
// edges, so non-producing cursors are unavoidable. The two orders fail on
// mirror-image shapes:
//
//   * order:"bfs"  -> CursorFifoQueue (FIFO). A single hub with a huge fan-out
//     queues all of its (leaf) cursors contiguously; the pop() that drains
//     them recurses once per leaf  -> WIDE graph triggers it.
//
//   * order:"dfs"  -> CursorLifoQueue (LIFO). Descending a long simple chain
//     pushes one cursor per level; on backtracking from the end every cursor
//     along the path is spent and contiguous, so the backtracking pop()
//     recurses once per level -> DEEP graph (with maxDepth >= chain length)
//     triggers it.
//
// On an unfixed build the bfs/dfs cases segfault the server. With pop()
// converted to a loop both queries complete and return the expected vertices.

const jsunity = require('jsunity');
const {assertEqual} = jsunity.jsUnity.assertions;

const db = require('internal').db;
const graphModule = require('@arangodb/general-graph');

// Recursion depth of the fatal pop() call; must exceed the scheduler thread's
// stack budget. 1,000,000 overflows comfortably across stack-size/optimization
// settings. With the fix the value is irrelevant (pop() uses O(1) stack).
const size = 1000 * 100;

// ===========================================================================
// BFS / CursorFifoQueue : WIDE graph (one hub -> many leaves)
// ===========================================================================
function bfsWideFanoutSuite() {
    const gn = 'UnitTestBfsWideGraph';
    const vn = 'UnitTestBfsWideVertex';
    const en = 'UnitTestBfsWideEdge';
    const hub = vn + '/hub';
    // maxDepth >= 2 so the depth-1 leaves themselves get a neighbour cursor.
    const maxDepth = 10;

    return {
        setUpAll: function () {
            graphModule._create(gn, [graphModule._relation(en, [vn], [vn])], []);

            db[vn].insert({_key: 'hub'});
            const batch = 100 * 1000;
            for (let start = 0; start < size; start += batch) {
                const end = Math.min(start + batch, size) - 1;
                db._query('FOR i IN @start..@end INSERT {_key: CONCAT("v", i)} INTO @@v',
                    {start: start, end: end, '@v': vn});
                db._query('FOR i IN @start..@end ' +
                    'INSERT {_from: @hub, _to: CONCAT(@vp, i)} INTO @@e',
                    {start: start, end: end, hub: hub, vp: vn + '/v', '@e': en});
            }
        },

        tearDownAll: function () {
            graphModule._drop(gn, true);
        },

        // Used to overflow the stack in CursorFifoQueue::pop() and crash the server.
        testBfsWideFanoutDoesNotOverflowStack: function () {
            const result = db._query(
                'FOR v IN 0..@d OUTBOUND @start GRAPH @g OPTIONS {order: "bfs"} RETURN v',
                {d: maxDepth, start: hub, g: gn}).toArray();
            // hub (depth 0) + every leaf (depth 1); leaves have no outgoing edges.
            assertEqual(1 + size, result.length);
        }
    };
}

// ===========================================================================
// DFS / CursorLifoQueue : DEEP graph (single chain v0 -> v1 -> ... -> vN)
// ===========================================================================
function dfsDeepChainSuite() {
    const gn = 'UnitTestDfsDeepGraph';
    const vn = 'UnitTestDfsDeepVertex';
    const en = 'UnitTestDfsDeepEdge';
    const start = vn + '/v0';
    // Must be >= chain length so the whole chain is descended (and then the
    // backtracking pop() recurses over every level at once).
    const maxDepth = size;

    return {
        setUpAll: function () {
            graphModule._create(gn, [graphModule._relation(en, [vn], [vn])], []);

            // vertices v0 .. v{size}, edges vi -> v{i+1}
            db._query('FOR i IN 0..2 INSERT {_key: CONCAT("v", i)} INTO @@v',
                {'@v': vn});
            db._query('FOR i IN 0..2 INSERT {_from: CONCAT(@vp, i), _to: CONCAT(@vp, (i + 1)%@s)} INTO @@e',
                {s: 3, vp: vn + '/v', '@e': en});
        },

        tearDownAll: function () {
            graphModule._drop(gn, true);
        },

        // Used to overflow the stack in CursorLifoQueue::pop() and crash the server.
        testDfsDeepChainDoesNotOverflowStack: function () {
            const result = db._query(
                'FOR v IN 0..@d OUTBOUND @start GRAPH @g OPTIONS {order: "dfs", uniqueEdges: "none", uniqueVertices: "none"} RETURN v',
                {d: maxDepth, start: start, g: gn}).toArray();
            // The chain v0..v{size} yields size + 1 vertices.
            assertEqual(size + 1, result.length);
        }
    };
}

jsunity.run(bfsWideFanoutSuite);
jsunity.run(dfsDeepChainSuite);
return jsunity.done();
