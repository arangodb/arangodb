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

const internal = require("internal");
const db = internal.db;
const sgm = require("@arangodb/smart-graph");
const cgm = require("@arangodb/general-graph");
const _ = require("lodash");

class TestGraph {
  constructor (graphName, edges, eRel, vn, en, vertexSharding, enterprise, options) {
    this.graphName = graphName;
    this.edges = edges;
    this.eRel = eRel;
    this.vn = vn;
    this.en = en;
    this.vertexSharding = vertexSharding;
    this.enterprise = enterprise;
    this.options = options || {};
  }

  create() {
    if (this.enterprise) {
      sgm._create(this.name(), [this.eRel], [], this.options);
    } else {
      cgm._create(this.name(), [this.eRel], [], this.options);
    }
    this.verticesByName = TestGraph._fillGraph(this.graphName, this.edges, db[this.vn], db[this.en], this.vertexSharding);
  }

  /**
   * @param gn Graph Name
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
      ec.save(v, w, {});
    }
    return verticesByName;
  }

  name() {
    return this.graphName;
  }

  vertex(name) {
    return this.verticesByName[name];
  }

  drop() {
    cgm._drop(this.name(), true);
  }
}

class ProtoGraph {
  static smartAttr() { return "smart"; }

  constructor (name, edges, generalShardings, smartShardings) {
    this.protoGraphName = name;
    this.edges = edges;
    this.generalShardings = generalShardings;

    // TODO: Col parameter?
    // const smartAttrsByShardIndex = this._smartAttrsPerShard();
    // this.smartShardings = smartShardings.map(([v, i]) => [v, smartAttrsByShardIndex[i]]);
  }

  prepareSingleServerGraph() {
    const vn = this.protoGraphName + '_Vertex';
    const en = this.protoGraphName + '_Edge';
    const gn = this.protoGraphName + '_Graph';
    this.gn = gn;
    const eRel = cgm._relation(en, vn, vn);

    return new TestGraph(gn, this.edges, eRel, vn, en, [], false);
  }

  name() {
    return this.protoGraphName;
  }

  prepareGeneralGraphs() {

    return this.generalShardings.map(numberOfShards =>  {
      const suffix = `_${numberOfShards}shards`;
      const vn = this.protoGraphName + '_Vertex' + suffix;
      const en = this.protoGraphName + '_Edge' + suffix;
      const gn = this.protoGraphName + '_Graph' + suffix;

      const eRel = cgm._relation(en, vn, vn);

      return new TestGraph(gn, this.edges, eRel, vn, en, [], false, {numberOfShards});
    });
  }

  prepareSmartGraphs() {
    return this.smartShardings.map(({numberOfShards, vertexSharding}) =>  {
      const suffix = `_${numberOfShards}shards`;
      const vn = this.protoGraphName + '_Vertex' + suffix;
      const en = this.protoGraphName + '_Edge' + suffix;
      const gn = this.protoGraphName + '_Graph' + suffix;

      const eRel = sgm._relation(en, vn, vn);
      const options = { numberOfShards, smartGraphAttribute: ProtoGraph.smartAttr() };

      return new TestGraph(gn, this.edges, eRel, vn, en, vertexSharding, true, options);
    });
  }

  _smartAttrsPerShard(col) {
    const shards = db[col].shards();

    // Create an array of size numberOfShards, each entry null.
    const exampleAttributeByShard = _.fromPairs(shards.map(id => [id, null]));

    const done = () => !
      _.values(exampleAttributeByShard)
      .includes(null);
    let i = 0;
    while (!done()) {
      const smartValue = i.toString();
      const doc = {[ProtoGraph.smartAttr()]: smartValue};

      const shard = col.getResponsibleShard(doc);
      if (exampleAttributeByShard[shard] !== null) {
        exampleAttributeByShard[shard] = smartValue;
      }

      ++i;
    }

    return _.values(exampleAttributeByShard);
  }

}

const protoGraphs = {

  /*
   *       B       E
   *     ↗   ↘   ↗
   *   A       D
   *     ↘   ↗   ↘
   *       C       F
   */
  openDiamond: new ProtoGraph("openDiamond", [
      ["A", "B"],
      ["A", "C"],
      ["B", "D"],
      ["C", "D"],
      ["D", "E"],
      ["D", "F"],
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
  ),
};

exports.ProtoGraph = ProtoGraph;
exports.protoGraphs = protoGraphs;
