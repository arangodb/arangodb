/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////


const tryRequire = (module) => {
  try {
    return require(module);
  } catch (e) {
  }
};

const internal = require("internal");
const db = internal.db;
const sgm = tryRequire("@arangodb/smart-graph");
const satgm = tryRequire("@arangodb/satellite-graph");
const cgm = require("@arangodb/general-graph");
const _ = require("lodash");
const assert = require("jsunity").jsUnity.assertions;


const TestVariants = Object.freeze({
  SingleServer: 1,
  GeneralGraph: 2,
  SmartGraph: 3,
  SatelliteGraph: 4,
  DisjointSmartGraph: 5
});

const graphWeightAttribute = 'distance';

class TestGraph {
  constructor(graphName, edges, eRel, vn, en, protoSmartSharding, testVariant, numberOfShards) {
    this.graphName = graphName;
    this.edges = edges;
    this.eRel = eRel;
    this.vn = vn;
    this.en = en;
    this.protoSmartSharding = protoSmartSharding;
    this.testVariant = testVariant;
    this.numberOfShards = numberOfShards;
  }

  create() {
    switch (this.testVariant) {
      case TestVariants.SingleServer: {
        cgm._create(this.name(), [this.eRel], [], {});
        break;
      }
      case TestVariants.GeneralGraph: {
        const options = {numberOfShards: this.numberOfShards};
        cgm._create(this.name(), [this.eRel], [], options);
        break;
      }
      case TestVariants.SmartGraph: {
        const options = {
          numberOfShards: this.numberOfShards,
          smartGraphAttribute: ProtoGraph.smartAttr(),
          isSmart: true
        };
        sgm._create(this.name(), [this.eRel], [], options);
        break;
      }
      case TestVariants.DisjointSmartGraph: {
        const options = {
          numberOfShards: this.numberOfShards,
          smartGraphAttribute: ProtoGraph.smartAttr(),
          isSmart: true,
          isDisjoint: true
        };
        sgm._create(this.name(), [this.eRel], [], options);
        break;
      }
      case TestVariants.SatelliteGraph: {
        const options = {
          replicationFactor: 'satellite'
        };
        satgm._create(this.name(), [this.eRel], [], options);
        break;
      }
    }

    if (this.testVariant === TestVariants.SingleServer) {
      this.verticesByName = TestGraph._fillGraph(this.graphName, this.edges, db[this.vn], db[this.en]);
    } else {
      const shardAttrsByShardIndex = this._shardAttrPerShard(db[this.vn]);
      const vertexSharding = this.protoSmartSharding.map(([v, i]) => [v, shardAttrsByShardIndex[i]]);
      this.verticesByName = TestGraph._fillGraph(this.graphName, this.edges, db[this.vn], db[this.en], vertexSharding);
    }
  }

  name() {
    return this.graphName;
  }

  weightAttribute() {
    return graphWeightAttribute;
  }

  vertex(name) {
    return this.verticesByName[name];
  }

  drop() {
    cgm._drop(this.name(), true);
  }

  /**
   * @param gn Graph Name
   * @param edges Array of pairs of strings, e.g. [["A", "B"], ["B", "C"]]
   * @param vc Vertex collection
   * @param ec Edge collection
   * @param vertexSharding Array of pairs, where the first element is the vertex
   *                       key and the second the smart attribute.
   * @private
   */
  static _fillGraph(gn, edges, vc, ec, vertexSharding = []) {
    const vertices = new Map(vertexSharding);
    for (const edge of edges) {
      if (!vertices.has(edge[0])) {
        vertices.set(edge[0], null);
      }
      if (!vertices.has(edge[1])) {
        vertices.set(edge[1], null);
      }
    }

    const verticesByName = {};
    for (const [vertexKey, smart] of vertices) {
      const doc = {key: vertexKey};
      if (smart !== null) {
        doc[ProtoGraph.smartAttr()] = smart;
      }
      verticesByName[vertexKey] = vc.save(doc)._id;
    }
    for (const edge of edges) {
      let v = verticesByName[edge[0]];
      let w = verticesByName[edge[1]];

      // check if our edge also has a weight defined and is a number
      if (edge[2] && typeof edge[2] === 'number') {
        // if found, add attribute "distance" as weightAttribute to the edge document
        let document = {};
        document[graphWeightAttribute] = edge[2];
        ec.save(v, w, document);
      } else {
        ec.save(v, w, {});
      }
    }
    return verticesByName;
  }

  _shardAttrPerShard(col) {
    const shards = col.shards();

    // Create an array of size numberOfShards, each entry null.
    const exampleAttributeByShard = _.fromPairs(shards.map(id => [id, null]));

    const done = () => !
      _.values(exampleAttributeByShard)
        .includes(null);
    let i = 0;
    // const key = this.enterprise ? ProtoGraph.smartAttr() : "_key";
    const key = "_key";
    while (!done()) {
      const value = this.enterprise ? i.toString() + ":" : i.toString();
      const doc = {[key]: value};

      let shard;
      try {
        shard = col.getResponsibleShard(doc);
      } catch (e) {
        throw new Error('Tried to get shard for ' + JSON.stringify(doc) + ', original error: ' + e);
      }
      if (exampleAttributeByShard[shard] === null) {
        exampleAttributeByShard[shard] = value;
      }

      ++i;
    }

    return _.values(exampleAttributeByShard);
  }
}

class ProtoGraph {
  static smartAttr() {
    return "smart";
  }

  constructor(name, edges, generalShardings, smartShardings) {
    this.protoGraphName = name;
    this.edges = edges;
    this.generalShardings = generalShardings;
    this.smartShardings = smartShardings;
  }

  name() {
    return this.protoGraphName;
  }

  prepareSingleServerGraph() {
    const vn = this.protoGraphName + '_Vertex';
    const en = this.protoGraphName + '_Edge';
    const gn = this.protoGraphName + '_Graph';
    const eRel = cgm._relation(en, vn, vn);

    return [new TestGraph(gn, this.edges, eRel, vn, en, [], TestVariants.SingleServer)];
  }

  prepareGeneralGraphs() {
    return this.generalShardings.map(numberOfShards => {
      const suffix = `_${numberOfShards}shards`;
      const vn = this.protoGraphName + '_Vertex' + suffix;
      const en = this.protoGraphName + '_Edge' + suffix;
      const gn = this.protoGraphName + '_Graph' + suffix;

      const eRel = cgm._relation(en, vn, vn);

      return new TestGraph(gn, this.edges, eRel, vn, en, [], TestVariants.GeneralGraph, numberOfShards);
    });
  }

  prepareSmartGraphs() {
    return this.smartShardings.map((sharding, idx) => {
      const {numberOfShards, vertexSharding} = sharding;
      const suffix = ProtoGraph._buildSmartSuffix(sharding, idx);

      const vn = this.protoGraphName + '_Vertex' + suffix;
      const en = this.protoGraphName + '_Edge' + suffix;
      const gn = this.protoGraphName + '_Graph' + suffix;

      const eRel = sgm._relation(en, vn, vn);

      return new TestGraph(gn, this.edges, eRel, vn, en, vertexSharding, TestVariants.SmartGraph, numberOfShards);
    });
  }

  prepareDisjointSmartGraphs() {
    return this.smartShardings.map((sharding, idx) => {
      const {numberOfShards, vertexSharding} = sharding;
      const suffix = ProtoGraph._buildSmartSuffix(sharding, idx);

      // All tests are based on fully connected graphs.
      // So just place all vertices on the same shard, no matter what.
      for (const pair of vertexSharding) {
        pair[1] = numberOfShards - 1;
      }

      const vn = this.protoGraphName + '_Vertex' + suffix;
      const en = this.protoGraphName + '_Edge' + suffix;
      const gn = this.protoGraphName + '_Graph' + suffix;

      const eRel = sgm._relation(en, vn, vn);

      return new TestGraph(gn, this.edges, eRel, vn, en, vertexSharding, TestVariants.DisjointSmartGraph, numberOfShards);
    });
  }

  prepareSatelliteGraphs() {
    // We're not able to test multiple shards in a SatelliteGraph as a SatelliteGraph has only one shard by default
    const suffix = '_satellite';
    const numberOfShards = 1;
    const vn = this.protoGraphName + '_Vertex' + suffix;
    const en = this.protoGraphName + '_Edge' + suffix;
    const gn = this.protoGraphName + '_Graph' + suffix;

    const eRel = cgm._relation(en, vn, vn);

    return [new TestGraph(gn, this.edges, eRel, vn, en, [], TestVariants.SatelliteGraph, numberOfShards)];
  }

  static _buildSmartSuffix({numberOfShards, vertexSharding, name}, shardingIndex) {
    if (name) {
      return `_${name}`;
    }

    // vertexSharding is an array of pairs, each pair holding a vertex (string)
    // and a shard (index), e.g. [["A", 0], ["B", 0], ["C", 1]].
    // For this (with 2 shards) this will return the string
    //   "_2shards_s0-AB_s1-C"
    let suffix = `_${numberOfShards}shards_` +
      _.toPairs(
        _.groupBy(vertexSharding, ([, s]) => s)
      ).map(
        ([s, vs]) => 's' + s + '-' + vs.map(([v,]) => v).join('')
      ).join('_');

    // Override long suffixes
    if (suffix.length > 40) {
      suffix = `_shardingNr${shardingIndex}`;
    }

    return suffix;
  }


}

const protoGraphs = {};

/*
 *       B       E
 *     ↗   ↘   ↗
 *   A       D
 *     ↘   ↗   ↘
 *       C       F
 */
protoGraphs.openDiamond = new ProtoGraph("openDiamond", [
    ["A", "B", 100],
    ["A", "C", 1],
    ["B", "D", 1],
    ["C", "D", 1],
    ["D", "E", 1],
    ["D", "F", 1],
  ],
  [1, 2, 5],
  [
    {
      numberOfShards: 1,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0],
          ["E", 0],
          ["F", 0],
        ],
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 0],
          ["D", 0],
          ["E", 0],
          ["F", 0],
        ],
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 1],
          ["E", 0],
          ["F", 0],
        ],
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0],
          ["E", 1],
          ["F", 1],
        ],
    },
    {
      numberOfShards: 6,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 2],
          ["D", 3],
          ["E", 4],
          ["F", 5],
        ],
    },
  ]
);

/*
 *       B
 *     ↗   ↘
 *   A       C
 *     ↖   ↙
 *       D
 */
protoGraphs.smallCircle = new ProtoGraph("smallCircle", [
    ["A", "B", 1],
    ["B", "C", 1],
    ["C", "D", 1],
    ["D", "A", 1]
  ],
  [1, 2, 5],
  [
    {
      numberOfShards: 1,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0]
        ]
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 0],
          ["D", 0]
        ]
    },
    {
      numberOfShards: 4,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 2],
          ["D", 3]
        ]
    },
  ]
);

/*
 *        B
 *     ↙↗ ↑  ↖↘
 *   A   ← →   C       // Same rules as in the picture for Node: "E"
 *     ↖↘ ↓  ↙↗
 *        D
 */
protoGraphs.completeGraph = new ProtoGraph("completeGraph", [
    ["A", "B", 2],
    ["A", "C", 5],
    ["A", "D", 5],
    ["A", "E", 1],
    ["B", "A", 1],
    ["B", "C", 2],
    ["B", "D", 5],
    ["B", "E", 5],
    ["C", "A", 5],
    ["C", "B", 1],
    ["C", "D", 2],
    ["C", "E", 5],
    ["D", "A", 5],
    ["D", "B", 5],
    ["D", "C", 2],
    ["D", "E", 2],
    ["E", "A", 2],
    ["E", "B", 5],
    ["E", "C", 5],
    ["E", "D", 1]
  ],
  [1, 2, 5],
  [
    {
      numberOfShards: 1,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0],
          ["E", 0],
        ]
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 0],
          ["D", 1],
          ["E", 0]
        ]
    },
    {
      numberOfShards: 5,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 2],
          ["D", 3],
          ["E", 4]
        ]
    },
  ]
);

/*
 *
 *
 * A → B → C → D → E → F → G → H → I → J
 *
 *
 */
protoGraphs.easyPath = new ProtoGraph("easyPath", [
    ["A", "B", 1],
    ["B", "C", 2],
    ["C", "D", 3],
    ["D", "E", 4],
    ["E", "F", 5],
    ["F", "G", 6],
    ["G", "H", 7],
    ["H", "I", 8],
    ["I", "J", 9]
  ],
  [1, 2, 5],
  [
    {
      numberOfShards: 1,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0],
          ["E", 0],
          ["F", 0],
          ["G", 0],
          ["H", 0],
          ["I", 0],
          ["J", 0]
        ]
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 0],
          ["D", 1],
          ["E", 0],
          ["F", 1],
          ["G", 0],
          ["H", 1],
          ["I", 0],
          ["J", 1]
        ]
    },
    {
      numberOfShards: 4,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 2],
          ["D", 3],
          ["E", 0],
          ["F", 1],
          ["G", 2],
          ["H", 3],
          ["I", 0],
          ["J", 1]
        ]
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0],
          ["E", 1],
          ["F", 1],
          ["G", 1],
          ["H", 1],
          ["I", 0],
          ["J", 0]
        ]
    },
  ]
);

/*
 *
 * ┌───────────↴   ┌───────────↴
 * A → B → C → D → E → F → G → H → I
 *
 *
 */
protoGraphs.advancedPath = new ProtoGraph("advancedPath", [
    ["A", "B", 1],
    ["A", "D", 10],
    ["B", "C", 1],
    ["C", "D", 1],
    ["D", "E", 1],
    ["E", "F", 1],
    ["E", "H", 10],
    ["F", "G", 1],
    ["G", "H", 1],
    ["H", "I", 1],
  ],
  [1, 2, 5],
  [
    {
      numberOfShards: 1,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0],
          ["E", 0],
          ["F", 0],
          ["G", 0],
          ["H", 0],
          ["I", 0]
        ]
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 1],
          ["E", 0],
          ["F", 0],
          ["G", 0],
          ["H", 1],
          ["I", 0],
          ["J", 0]
        ]
    },
    {
      numberOfShards: 4,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 2],
          ["D", 3],
          ["E", 0],
          ["F", 1],
          ["G", 2],
          ["H", 3],
          ["I", 0],
          ["J", 1]
        ]
    },
  ]
);

/*
 * ┌───────────────────────┐(to G)
 * ├───────────↴   ┌──────┄↓┄──↴
 * A → B → C → D → E → F → G → H → I
 *
 *
 */
protoGraphs.moreAdvancedPath = new ProtoGraph("moreAdvancedPath", [
    ["A", "B"],
    ["A", "D"],
    ["A", "G"],
    ["B", "C"],
    ["C", "D"],
    ["D", "E"],
    ["E", "F"],
    ["E", "H"],
    ["F", "G"],
    ["G", "H"],
    ["H", "I"],
  ],
  [1, 2, 5],
  [
    {
      numberOfShards: 1,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 0],
          ["E", 0],
          ["F", 0],
          ["G", 0],
          ["H", 0],
          ["I", 0]
        ]
    },
    {
      numberOfShards: 2,
      vertexSharding:
        [
          ["A", 0],
          ["B", 0],
          ["C", 0],
          ["D", 1],
          ["E", 0],
          ["F", 0],
          ["G", 0],
          ["H", 1],
          ["I", 0],
          ["J", 0]
        ]
    },
    {
      numberOfShards: 4,
      vertexSharding:
        [
          ["A", 0],
          ["B", 1],
          ["C", 2],
          ["D", 3],
          ["E", 0],
          ["F", 1],
          ["G", 2],
          ["H", 3],
          ["I", 0],
          ["J", 1]
        ]
    },
  ]
);

/*
 * Perfect binary tree of depth 8 (i.e. 9 levels).
 * Vertices get the names "v0", "v1", ..., "v510".
 * v0 is the root vertex. Edges are (v0, v1), (v0, v2), (v1, v3), ...
 * Contains 511 vertices and 510 edges.
 */
{ // scope for largeBinTree-local variables
  const numVertices = Math.pow(2, 9) - 1;
  const parentIdx = (i) => _.floor((i - 1) / 2);
  const vertices = _.range(0, numVertices)
    .map(i => `v${i}`);
  const edges = _.range(1, numVertices)
    .map(i => [`v${parentIdx(i)}`, `v${i}`]);

  assert.assertEqual(511, vertices.length);
  assert.assertEqual(510, edges.length);
  assert.assertEqual('v0', vertices[0]);
  assert.assertEqual('v510', vertices[vertices.length - 1]);

  const vi = (v) => Number(v.match(/^v(\d+)$/)[1]);
  const vertexLevel = (v) => Math.floor(Math.log2(vi(v) + 1));
  const parent = (v) => 'v' + parentIdx(vi(v));
  const ancestor = (v, i) => i === 0 ? v : ancestor(parent(v), i - 1);

  // when splitting the tree into perfect subtrees of depth 3 (this is
  // non-ambiguous), these helper functions return, for a given vertex,
  // the level inside its subtree, and its subtrees root, respectively.
  const subTreeD3Level = (v) => vertexLevel(v) % 3;
  const subTreeD3Root = (v) => ancestor(v, subTreeD3Level(v));

  // The subtree roots are all at (2^3)^n-1..2*(2^3)^n-1 (not including the last)
  // all together 73 subtrees (1 + 8 + 64)
  const subTreeD3Roots = [0, ..._.range(7, 15), ..._.range(63, 127)].map(i => `v${i}`);
  const subTreeD3RootToShardIdx = new Map(subTreeD3Roots.map((v, i) => [v, i]));

  assert.assertEqual(73, subTreeD3Roots.length);

  protoGraphs.largeBinTree = new ProtoGraph("largeBinTree",
    edges,
    [1, 2, 5],
    [
      { // one shard
        name: "oneShard",
        numberOfShards: 1,
        vertexSharding: vertices.map(v => [v, 0]),
      },
      { // one shard per three levels
        name: "oneShardPerThreeLevels",
        numberOfShards: 3,
        vertexSharding: vertices.map(v => [v, Math.floor(vertexLevel(v) / 3)]),
      },
      { // one shard per level
        name: "oneShardPerLevel",
        numberOfShards: 9,
        vertexSharding: vertices.map(v => [v, vertexLevel(v)]),
      },
      { // alternating distribution of vertices
        name: "alternatingSharding",
        numberOfShards: 5,
        vertexSharding: vertices.map(v => [v, vi(v) % 5]),
      },
      { // alternating sequence distribution of vertices
        name: "alternatingSequenceSharding",
        numberOfShards: 5,
        vertexSharding: vertices.map(v => [v, Math.floor(vi(v) / 11) % 5]),
      },
      { // most vertices in 0, but for a diagonal cut through the tree:
        //                        v0
        //            v1                     (v2)
        //      v3         (v4)         v5          v6
        //   v7   (v8)   v9   v10   v11   v12   v13   v14
        //  ...
        name: "diagonalCutSharding",
        numberOfShards: 2,
        vertexSharding: vertices.map(v => [v, [2, 4, 8, 16, 32, 64, 128, 256].includes(vi(v)) ? 1 : 0]),
      },
      { // perfect subtrees of depth 3, each in different shards
        name: "perfectSubtreeSharding",
        numberOfShards: 73,
        vertexSharding: vertices.map(v => [v, subTreeD3RootToShardIdx.get(subTreeD3Root(v))]),
      },
      { // perfect subtrees of depth 3 as above, but divided in fewer shards
        name: "perfectSubtreeShardingWithFewShards",
        numberOfShards: 5,
        vertexSharding: vertices.map(v => [v, subTreeD3RootToShardIdx.get(subTreeD3Root(v)) % 5]),
      },
    ]
  );
}

exports.ProtoGraph = ProtoGraph;
exports.protoGraphs = protoGraphs;
