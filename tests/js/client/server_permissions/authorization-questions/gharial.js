/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

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
// //////////////////////////////////////////////////////////////////////////////

// Authorization questions asked by the /_api/gharial endpoint family.
//
// Observation-based counterpart of tests/api/apitests/gharial.mjs.
//
// Handlers: arangod/RestHandler/RestGraphHandler.cpp
//           arangod/Graph/GraphManager.cpp
//           arangod/Graph/GraphOperations.cpp
//
// Every request first asks `UseDatabase name=d level=read`
// (RestHandler::checkUserCanAccess).
//
// Any request that names a graph in the path first runs RestGraphHandler::
// getGraph() -> GraphManager::lookupGraphByName(), which asks:
//     UseGraph db=d name=<g> level=read                (canUseGraph Read)
//     UseCollection db=d name=_graphs level=read       (READ txn on _graphs)
// We refer to this pair as the "lookup preamble" below.
//
// Beyond that, per operation (ExecContext helpers in ExecContext.cpp and the
// transaction layer in TransactionState::checkCollectionPermission):
//   list graphs        -> readGraphs(): _graphs read + canSeeGraph per graph
//   create graph       -> canCreateGraph(collectionNamesToCreate/ToRead=[..])
//   drop graph         -> canDropGraph(collectionNames=[..])
//   read edge/vertex   -> READ txn on the data collection  -> UseCollection read
//   write edge/vertex  -> WRITE txn on the data collection  -> UseCollection writedata
//   edge insert/replace-> validateEdge reads _from/_to (c) + writes edge (e)
//   structure changes  -> canUseGraph(Modify) + WRITE txn on _graphs
//
// NOTE: every collection a transaction opens is loaded via
// Database::loadCollection(), which unconditionally asks
// `UseCollection ... level=read` (vocbase.cpp:387) in addition to the
// writedata question from TransactionState::checkCollectionPermission. So a
// write to a collection yields BOTH a read and a writedata question for it
// (e.g. writing edge e -> read e + writedata e; the _graphs writes likewise
// carry the _graphs read already present in the lookup preamble).
//
// AUDIT: the gharial handler chains several sub-operations (graph lookups, the
// _graphs system-collection transactions, ensureAllCollections lookups,
// applyOnAllGraphs / readGraphs enumerations, and a trailing getGraph() to
// build the response). The expected sets below list the questions derived from
// the source but the exact multiplicity/ordering for the structural operations
// (create/drop graph, add/edit/remove edge definition, add/remove orphan) is
// error-prone and must be audited.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB,
  DOC_COLLECTION,
  EDGE_COLLECTION,
  GRAPH,
  singleOnly
} = require('@arangodb/testutils/apitest-fixtures');

function gharialApiAuthzSuite () {
  const c = DOC_COLLECTION;               // 'c'
  const e = EDGE_COLLECTION;              // 'e'
  const g = GRAPH;                        // 'g'

  const G_APITEST = 'g_apitest';
  const E_APITEST = 'e_apitest';
  const E2_APITEST = 'e2_apitest';
  const C_ORPHAN = 'c_orphan_apitest';
  const EDGE_KEY = 'e_apitest_doc';
  const VERTEX_KEY = 'v_apitest_doc';

  // ---- raw setup / teardown helpers (run as root, before beginObserve) ----

  function rawDelDoc (coll, key) {
    arango.DELETE_RAW(`/_db/${DB}/_api/document/${coll}/${key}`);
  }
  function insertTestEdge () {
    rawDelDoc(e, EDGE_KEY);
    arango.POST_RAW(`/_db/${DB}/_api/document/${e}`,
                    { _key: EDGE_KEY, _from: `${c}/k1`, _to: `${c}/k2` });
  }
  function insertTestVertex () {
    rawDelDoc(c, VERTEX_KEY);
    arango.POST_RAW(`/_db/${DB}/_api/document/${c}`,
                    { _key: VERTEX_KEY, value: 9999 });
  }
  function delGraph (name) {
    arango.DELETE_RAW(`/_db/${DB}/_api/gharial/${name}?dropCollections=false`);
  }
  function delColl (name) {
    arango.DELETE_RAW(`/_db/${DB}/_api/collection/${name}`);
  }
  function createGapitest () {
    delGraph(G_APITEST);
    delColl(E_APITEST);
    arango.POST_RAW(`/_db/${DB}/_api/collection`, { name: E_APITEST, type: 3 });
    arango.POST_RAW(`/_db/${DB}/_api/gharial`, {
      name: G_APITEST,
      edgeDefinitions: [{ collection: E_APITEST, from: [c], to: [c] }]
    });
  }
  function createGapitestWithE2 () {
    createGapitest();
    delColl(E2_APITEST);
    arango.POST_RAW(`/_db/${DB}/_api/collection`, { name: E2_APITEST, type: 3 });
  }
  function createGapitestWithOrphan () {
    createGapitest();
    delColl(C_ORPHAN);
    arango.POST_RAW(`/_db/${DB}/_api/collection`, { name: C_ORPHAN, type: 2 });
    arango.POST_RAW(`/_db/${DB}/_api/gharial/${G_APITEST}/vertex`,
                    { collection: C_ORPHAN });
  }
  function createGapitestForAddOrphan () {
    createGapitest();
    delColl(C_ORPHAN);
    arango.POST_RAW(`/_db/${DB}/_api/collection`, { name: C_ORPHAN, type: 2 });
  }
  function cleanupTemp () {
    delGraph(G_APITEST);
    delColl(E_APITEST);
    delColl(E2_APITEST);
    delColl(C_ORPHAN);
    rawDelDoc(e, EDGE_KEY);
    rawDelDoc(c, VERTEX_KEY);
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
      cleanupTemp();
    },

    // GET /_api/gharial - graphActionReadGraphs() -> GraphManager::readGraphs():
    // an AQL read over _graphs (=> _graphs read) then canSeeGraph() per graph.
    // AUDIT: enumeration of graphs (only 'g' in the fixture); the _graphs read
    //        stems from the AQL query.
    testListGraphs: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/gharial`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=_graphs level=read",
        "SeeGraph db=d name=g"
      ], endObserve());
    },

    // POST /_api/gharial - graphActionCreateGraph() -> GraphManager::createGraph:
    //   graphExists()                 -> _graphs read
    //   checkCreateGraphPermissions() -> CreateGraph ...
    //   ensureAllCollections()        -> lookup() per existing collection (read)
    //   storeGraph()                  -> _graphs writedata
    //   getGraph() (build response)   -> lookup preamble again
    // e_apitest is pre-created and c exists, so both are "to read".
    // AUDIT: collectionNamesToRead ordering ([edge..., vertex...]) and the
    //        exact set of ensureAllCollections lookups are uncertain.
    testCreateGraph: function () {
      delGraph(G_APITEST);
      delColl(E_APITEST);
      arango.POST_RAW(`/_db/${DB}/_api/collection`, { name: E_APITEST, type: 3 });
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/gharial`, {
        name: G_APITEST,
        edgeDefinitions: [{ collection: E_APITEST, from: [c], to: [c] }]
      });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "CreateGraph db=d name=g_apitest collectionNamesToCreate=[] collectionNamesToRead=[e_apitest,c]",
        "UseCollection db=d name=e_apitest level=read",
        "UseCollection db=d name=c level=read",
        "UseCollection db=d name=_graphs level=writedata",
        "UseGraph db=d name=g_apitest level=read"
      ], endObserve());
    },

    // GET /_api/gharial/g - graphActionReadGraphConfig(): only the lookup
    // preamble (config is served from the already-loaded graph object).
    testGetGraph: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/gharial/${g}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read"
      ], endObserve());
    },

    // DELETE /_api/gharial/g_apitest?dropCollections=false -
    // graphActionRemoveGraph() -> GraphManager::removeGraph:
    //   checkDropGraphPermissions() -> DropGraph collectionNames=[] (nothing to
    //     drop because dropCollections=false), then WRITE txn on _graphs.
    testDropGraph: function () {
      createGapitest();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/gharial/${G_APITEST}?dropCollections=false`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g_apitest level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "DropGraph db=d name=g_apitest collectionNames=[]",
        "UseCollection db=d name=_graphs level=writedata"
      ], endObserve());
    },

    // GET /_api/gharial/g/edge - graphActionReadConfig(): lookup preamble only.
    testListEdgeDefinitions: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/gharial/${g}/edge`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read"
      ], endObserve());
    },

    // POST /_api/gharial/g_apitest/edge - addEdgeDefinition():
    //   canUseGraph(Modify), then ensureAllCollections() which looks up every
    //   edge and vertex collection of the (now-extended) graph -> e_apitest,
    //   e2_apitest (edges) and c (vertex), each a UseCollection(Read); then
    //   storeGraph() (WRITE txn on _graphs) and a trailing getGraph().
    // AUDIT: ensureAllCollections lookup set/ordering + trailing getGraph
    //        preamble multiplicity need verification.
    testAddEdgeDefinition: function () {
      createGapitestWithE2();
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/gharial/${G_APITEST}/edge`,
                      { collection: E2_APITEST, from: [c], to: [c] });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g_apitest level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseGraph db=d name=g_apitest level=modify",
        "UseCollection db=d name=e_apitest level=read",
        "UseCollection db=d name=e2_apitest level=read",
        "UseCollection db=d name=c level=read",
        "UseCollection db=d name=_graphs level=writedata"
      ], endObserve());
    },

    // GET /_api/gharial/g/edge/e/{key} - edgeActionRead() -> READ txn on e.
    testReadEdge: function () {
      insertTestEdge();
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/gharial/${g}/edge/${e}/${EDGE_KEY}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "UseCollection db=d name=e level=read"
      ], endObserve());
    },

    // POST /_api/gharial/g/edge/e - createEdge() -> validateEdge() opens a txn
    // with the _from/_to vertex collection (c) as read and the edge collection
    // (e) as write.
    testInsertEdge: function () {
      rawDelDoc(e, EDGE_KEY);
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/gharial/${g}/edge/${e}`,
                      { _key: EDGE_KEY, _from: `${c}/k1`, _to: `${c}/k2` });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "UseCollection db=d name=c level=read",
        "IsReadOnly",
        "UseCollection db=d name=e level=writedata",
        ...singleOnly([
          "UseCollection db=d name=e level=read"
        ])
      ], endObserve());
    },

    // PUT /_api/gharial/g_apitest/edge/e_apitest - editEdgeDefinition():
    //   canUseGraph(Modify), checkEdgeDefinitionPermissions() (c + e_apitest
    //   read), findOrCreateCollectionsByEdgeDefinition, readGraphs(), WRITE txn
    //   on _graphs, trailing getGraph().
    // AUDIT: composite; verify the enumeration/preamble multiplicity.
    testReplaceEdgeDefinition: function () {
      createGapitest();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/gharial/${G_APITEST}/edge/${E_APITEST}`,
                     { collection: E_APITEST, from: [c], to: [c] });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseDatabase name=d level=write",
        "UseGraph db=d name=g_apitest level=read",
        "UseCollection db=d name=_graphs level=read",
        "UseGraph db=d name=g_apitest level=modify",
        "SeeGraph db=d name=g",
        "SeeGraph db=d name=g_apitest",
        "UseCollection db=d name=c level=read",
        "UseCollection db=d name=e_apitest level=read",
        "UseCollection db=d name=_graphs level=writedata"
      ], endObserve());
    },

    // DELETE /_api/gharial/g_apitest/edge/e_apitest?dropCollection=false -
    // eraseEdgeDefinition(): canUseGraph(Modify) + WRITE txn on _graphs
    // (dropCollection=false, so no collection RW check).
    testRemoveEdgeDefinition: function () {
      createGapitest();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/gharial/${G_APITEST}/edge/${E_APITEST}?dropCollection=false`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g_apitest level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseGraph db=d name=g_apitest level=modify",
        "UseCollection db=d name=_graphs level=writedata"
      ], endObserve());
    },

    // PUT /_api/gharial/g/edge/e/{key} - replaceEdge() -> validateEdge() (c
    // read, e write) + modifyDocument. Body carries _from/_to (=> c read).
    testReplaceEdge: function () {
      insertTestEdge();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/gharial/${g}/edge/${e}/${EDGE_KEY}`,
                     { _from: `${c}/k2`, _to: `${c}/k3` });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "UseCollection db=d name=c level=read",
        "IsReadOnly",
        "UseCollection db=d name=e level=writedata",
        ...singleOnly([
          "UseCollection db=d name=e level=read"
        ])
      ], endObserve());
    },

    // PATCH /_api/gharial/g/edge/e/{key} - updateEdge() -> validateEdge(). Body
    // ({extra:1}) has no _from/_to, so no vertex collection is added to the txn;
    // only the edge collection (e) is opened -> read (loadCollection) + writedata.
    // AUDIT: absence of the c read hinges on the body carrying no _from/_to.
    testUpdateEdge: function () {
      insertTestEdge();
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/gharial/${g}/edge/${e}/${EDGE_KEY}`,
                       { extra: 1 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseCollection db=d name=e level=writedata",
        ...singleOnly([
          "UseCollection db=d name=e level=read"
        ])
      ], endObserve());
    },

    // DELETE /_api/gharial/g/edge/e/{key} - removeEdge() -> removeEdgeOrVertex:
    //   applyOnAllGraphs() (=> _graphs read + canSeeGraph per graph) then a
    //   WRITE txn over the edge collection e (plus an AQL to purge dangling
    //   edges).
    // AUDIT: applyOnAllGraphs enumeration (only 'g' present here).
    testDeleteEdge: function () {
      insertTestEdge();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/gharial/${g}/edge/${e}/${EDGE_KEY}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseCollection db=d name=e level=writedata",
        ...singleOnly([
          "UseCollection db=d name=e level=read"
        ])
      ], endObserve());
    },

    // GET /_api/gharial/g/vertex - graphActionReadConfig(): lookup preamble.
    testListVertexCollections: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/gharial/${g}/vertex`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read"
      ], endObserve());
    },

    // POST /_api/gharial/g_apitest/vertex - addOrphanCollection():
    //   canUseGraph(Modify), ensureAllCollections(), WRITE txn on _graphs,
    //   trailing getGraph().
    // AUDIT: composite; ensureAllCollections lookups + preamble multiplicity.
    testAddOrphanCollection: function () {
      createGapitestForAddOrphan();
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/gharial/${G_APITEST}/vertex`,
                      { collection: C_ORPHAN });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g_apitest level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseGraph db=d name=g_apitest level=modify",
        "UseCollection db=d name=c level=read",
        "UseCollection db=d name=e_apitest level=read",
        "UseCollection db=d name=c_orphan_apitest level=read",
        "UseCollection db=d name=_graphs level=writedata"
      ], endObserve());
    },

    // GET /_api/gharial/g/vertex/c/k1 - vertexActionRead() -> READ txn on c.
    testReadVertex: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/gharial/${g}/vertex/${c}/k1`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // POST /_api/gharial/g/vertex/c - createVertex() -> WRITE txn on c.
    testInsertVertex: function () {
      rawDelDoc(c, VERTEX_KEY);
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/gharial/${g}/vertex/${c}`,
                      { _key: VERTEX_KEY, value: 9999 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // DELETE /_api/gharial/g_apitest/vertex/c_orphan?dropCollection=false -
    // eraseOrphanCollection(): canUseGraph(Modify) + WRITE txn on _graphs.
    testRemoveOrphanCollection: function () {
      createGapitestWithOrphan();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/gharial/${G_APITEST}/vertex/${C_ORPHAN}?dropCollection=false`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g_apitest level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseGraph db=d name=g_apitest level=modify",
        "UseCollection db=d name=c_orphan_apitest level=writemeta",
        "UseCollection db=d name=_graphs level=writedata"
      ], endObserve());
    },

    // PUT /_api/gharial/g/vertex/c/{key} - replaceVertex() -> WRITE txn on c.
    testReplaceVertex: function () {
      insertTestVertex();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/gharial/${g}/vertex/${c}/${VERTEX_KEY}`,
                     { value: 10000 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // PATCH /_api/gharial/g/vertex/c/{key} - updateVertex() -> WRITE txn on c.
    testUpdateVertex: function () {
      insertTestVertex();
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/gharial/${g}/vertex/${c}/${VERTEX_KEY}`,
                       { extra: 42 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read"
        ])
      ], endObserve());
    },

    // DELETE /_api/gharial/g/vertex/c/{key} - removeVertex() ->
    // removeEdgeOrVertex: applyOnAllGraphs (=> _graphs read + canSeeGraph per
    // graph) then a WRITE txn over the vertex collection c AND every edge
    // collection of the graph (e), to purge incident edges.
    // AUDIT: applyOnAllGraphs enumeration; e is locked to remove dangling edges.
    testDeleteVertex: function () {
      insertTestVertex();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/gharial/${g}/vertex/${c}/${VERTEX_KEY}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseGraph db=d name=g level=read",
        "UseCollection db=d name=_graphs level=read",
        "IsReadOnly",
        "UseCollection db=d name=c level=writedata",
        "UseCollection db=d name=e level=writedata",
        ...singleOnly([
          "UseCollection db=d name=c level=read",
          "UseCollection db=d name=e level=read"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(gharialApiAuthzSuite);
return jsunity.done();
