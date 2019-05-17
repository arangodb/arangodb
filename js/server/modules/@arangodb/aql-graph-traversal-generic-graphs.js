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

//
// const vn = "UnitTestVertexCollection";
// const en = "UnitTestEdgeCollection";
// const eRel = gm._relation(en, vn, vn);
// const options = { numberOfShards: 4, smartGraphAttribute: 'smart' };


class ProtoGraph {
  static smartAttr() { return "smart"; }

  constructor (name, edges, generalShardings, smartShardings) {
    this.name = name;
    this.edges = edges;
    this.generalShardings = generalShardings;
    this.smartShardings = smartShardings;
  }

  createSingleServerGraph() {
    const vn = this.name + '_Vertex';
    const en = this.name + '_Edge';
    const gn = this.name + '_Graph';
    const eRel = cgm._relation(en, vn, vn);
    cgm._create(gn, eRel);

    this._fillGraph(db[vn], db[en]);

    return gn;
  }

  createGeneralGraphs() {
    return this.generalShardings.map(numberOfShards =>  {
      const suffix = `_${numberOfShards}shards`;
      const vn = this.name + '_Vertex' + suffix;
      const en = this.name + '_Edge' + suffix;
      const gn = this.name + '_Graph' + suffix;
      const eRel = cgm._relation(en, vn, vn);
      const options = { numberOfShards };
      cgm._create(gn, eRel, [], options);

      this._fillGraph(db[vn], db[en]);

      return gn;
    });
  }

  createSmartGraphs() {
    return this.smartShardings.map(([numberOfShards, vertexSharding]) =>  {
      const suffix = `_${numberOfShards}shards`;
      const vn = this.name + '_Vertex' + suffix;
      const en = this.name + '_Edge' + suffix;
      const gn = this.name + '_Graph' + suffix;
      const eRel = sgm._relation(en, vn, vn);
      const options = { numberOfShards, smartGraphAttribute: ProtoGraph.smartAttr() };
      sgm._create(gn, eRel, [], options);

      // TODO translate vertexSharding with _smartAttrsPerShard()

      this._fillGraph(db[vn], db[en], vertexSharding);

      return gn;
    });
  }

  _smartAttrsPerShard(col) {
    const shards = col.shards();

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

  /**
   * @param vc Vertex collection
   * @param ec Edge collection
   * @param vertexSharding Array of pairs, where the first element is the vertex
   *                       key and the second the smart attribute.
   * @private
   */
  _fillGraph(vc, ec, vertexSharding = []) {
    const vertices = new Map(vertexSharding);
    for (const edge of this.edges) {
      if (!vertices.has(edge[0])) {
        vertices.add(edge[0], null);
      }
      if (!vertices.has(edge[1])) {
        vertices.add(edge[1], null);
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
    for (const edge of this.edges) {
      let v = verticesByName[edge[0]];
      let w = verticesByName[edge[1]];
      ec.save(v, w, {});
    }
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
        vertexShardings:
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
        vertexShardings:
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
        vertexShardings:
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
        vertexShardings:
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
        vertexShardings:
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
