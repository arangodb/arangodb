/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Julia Volmer
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const arango = require("internal").arango;
const graph_module = require("@arangodb/general-graph");

const graphName = "UnitTest_pregel";
const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function assertPregelStarted(response) {
	assertEqual(response.code, 200);
	// returns execution number of created pregel run
	assertTrue(!isNaN(parseInt(response.parsedBody, 10)));
}

function start_pregel_run() {
	return {
		setUp: function () {
			var graph = graph_module._create(graphName);
			db._create(vColl, { numberOfShards: 4 });
			db._createEdgeCollection(eColl, {
				numberOfShards: 4,
				replicationFactor: 1,
				shardKeys: ["vertex"],
				distributeShardsLike: vColl
			});
			graph._addVertexCollection(vColl);
			var rel = graph_module._relation(eColl, [vColl], [vColl]);
			graph._extendEdgeDefinitions(rel);
		},

		tearDown: function () {
			graph_module._drop(graphName, true);
		},


		test_pregel_accepts_graph: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "graphName": graphName });
			assertPregelStarted(response);
		},
		test_pregel_accepts_collections: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "vertexCollections": [vColl], "edgeCollections": [eColl] });
			assertPregelStarted(response);
		},
		test_pregel_fails_with_only_edge_collections: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "edgeCollections": [eColl] });
			assertEqual(response.code, 400);
		},
		test_pregel_fails_with_only_vertex_collections: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "vertexCollections": [eColl] });
			assertEqual(response.code, 400);
		},
		test_pregel_accepts_vertex_collections_and_graph: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "vertexCollections": [vColl], "graphName": graphName });
			assertPregelStarted(response);
		},
		test_pregel_accepts_edge_collections_and_graph: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "edgeCollections": [eColl], "graphName": graphName });
			assertPregelStarted(response);
		},
		test_pregel_accepts_vertex_and_edge_collections_and_graph: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "vertexCollections": [vColl], "edgeCollections": [eColl], "graphName": graphName });
			assertPregelStarted(response);
		},
		test_pregel_accepts_empty_vertex_and_edge_collection_arrays: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "vertexCollections": [], "edgeCollections": [] });
			assertPregelStarted(response);
		},
		test_pregel_fails_without_algorithm: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "graphName": graphName });
			assertEqual(response.code, 404);
		},
		test_pregel_accepts_parameters: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "graphName": graphName, "params": {} });
			assertPregelStarted(response);
		},
		test_pregel_accepts_crap: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "graphName": graphName, "someotherstuff": "bla" });
			assertPregelStarted(response);
		},

		test_pregel_fails_with_empty_graphName: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "wcc", "graphName": "" });
			assertEqual(response.code, 400);
		},
		test_pregel_fails_with_unsupported_algorithm: function () {
			const response = arango.POST_RAW("/_api/control_pregel", { "algorithm": "non-implemented-algorithm", "graphName": graphName });
			assertEqual(response.code, 400);
		},

	};
}
jsunity.run(start_pregel_run);
return jsunity.done();
