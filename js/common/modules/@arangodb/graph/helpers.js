/* jshint strict: true, unused: true */
/* global AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief Helpers for graph tests
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Max Neunhoeffer
/// @author Copyright 2016-2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
const db = require("@arangodb").db;
const {assertEqual, assertNotEqual} = jsunity.jsUnity.assertions;

function makeTree(k, depth, nrShards, subgraph) {
  // This creates a large graph (vertices and edges), which is a k-ary
  // tree of depth <depth>. If <subgraph> is 'random', then the subgraph
  // attribute 'sub' will be set randomly in [1..nrShards] as string.
  // If <subgraph> is ['prefix', d], then the subgraph attribute 'sub'
  // will be set to 'top' for the layers of depth < d and [1..2^d] as
  // string for the 2^d subtrees starting at depth d. If <subgraph> is
  // 'depth', then the subgraph attribute 'sub' will be set to the
  // depth of the vertex in the tree as string. Returns an object with
  // 'vertices' and 'edges' attribute.
  if (typeof k !== 'number' || k > 9 || k < 2) {
    throw 'bad k';
  }
  let r = { 'vertices': [], 'edges': [], k, depth, nrShards, subgraph };
  const makeVertices = (which, d) => {
    let v = {name: which};
    if (subgraph === 'random') {
      v.sub = '' + Math.floor(Math.random() * nrShards + 1.0);
    } else if (subgraph === 'depth') {
      v.sub = '' + depth;
    } else {
      if (d < subgraph[1]) {
        v.sub = 'top';
      } else {
        v.sub = which.substr(0, subgraph[1] + 1);
      }
    }
    let pos = r.vertices.length;
    r.vertices.push(v);
    if (d >= depth) {
      return pos;
    }
    for (let i = 0; i < k; i++) {
      let subpos = makeVertices(which + i, d+1);
      let e = { _from: pos, _to: subpos, name: '->' + which + i };
      r.edges.push(e);
    }
    return pos;
  };
  makeVertices('N', 0);
  return r;
}

class PoorMansRandom {
  constructor(seed) {
    this.x = seed === undefined ? 1234567 : seed;
  }
  next() {
    this.x = Math.floor(this.x * 15485867 + 17263) % 10000000;
    return this.x;
  }
  nextUnitInterval() {
    return this.next() / 10000000.0;
  }
}

function makeClusteredGraph(clusterSizes, intDegree, intDegreeDelta,
                                          extDegree, seed) {
  // This creates a pseudo-random clustered graph. The arguments are:
  // clusterSizes:     array of positive integers, sizes of the subgraphs
  // intDegree:        average number of edges from a node to an other within
  //                   the same subgraph
  // intDegreeDelta:   the actual number of such edges is random between
  //                   intDegree - intDegreeDelta and intDegree + intDegreeDelta
  // extDegree:        number between 0 and 1.0, pseudo-probability that  a
  //                   vertex has an edge to some other subgraph
  // seed:             random seed

  // Just to make the first few cluster names a bit more interesting:
  let clusterNames = ['ABC', 'ZTG', 'JSH', 'LIJ', 'GTE', 'NSD', 'POP', 'GZU',
                      'RRR', 'WER', 'UIH', 'KLO', 'QWE', 'FAD', 'MNB', 'VFR'];
  // Just to make the first 10000 vertex names in a cluster a bit more
  // interesting:
  let names1 = ['Max', 'Ulf', 'Andreas', 'Kaveh', 'Claudius', 'Jan',
                'Michael', 'Oliver', 'Frank', 'Willi'];
  let names2 = ['Friedhelm', 'Habakuk', 'Mickey', 'Karl-Heinz',
                'Friedrich-Wilhelm', 'Hans-Guenter', 'Hades', 'Callibso',
                'Jupiter', 'Odin'];
  let names3 = ['Chaplin', 'Hardy', 'Laurel', 'Keaton', 'Hallervorden',
                'Appelt', 'Mittermaier', 'Trump', 'Queen Elizabeth',
                'Merkel'];
  function makeName(i) {
    let r = names1[i % 10] + '-' + names2[Math.floor(i / 10) % 10] + '_' +
            names3[Math.floor(i / 100) % 10];
    let z = Math.floor(i / 1000);
    return z > 0 ? r + z : r;
  }
          
  let rand = new PoorMansRandom(seed);
  let r = { 'vertices': [], 'edges': [], clusterSizes,
            intDegree, intDegreeDelta, extDegree, seed };

  // Make the vertices:
  let clusters = [];
  for (let i = 0; i < clusterSizes.length; ++i) {
    let cluster = [];
    let subgraphId = i < clusterNames.length ? clusterNames[i] : 'C' + i;
    for (let j = 0; j < clusterSizes[i]; ++j) {
      let v = {subgraphId, name: subgraphId + '_' + makeName(j)};
      cluster.push({v, pos: r.vertices.length});
      r.vertices.push(v);
    }
    clusters.push(cluster);
  }

  // Make the edges:
  for (let i = 0; i < clusterSizes.length; ++i) {
    let cluster = clusters[i];
    let subgraphId = i < clusterNames.length ? clusterNames[i] : 'C' + i;
    for (let j = 0; j < cluster.length; ++j) {
      let vv = cluster[j];
      let v = vv.v;
      let nrInt = intDegree - intDegreeDelta + 
                  rand.next() % (2 * intDegreeDelta);
      for (let k = 0; k < nrInt; ++k) {
        let toVertexPos = rand.next() % cluster.length;
        let toVertex = r.vertices[cluster[toVertexPos].pos];
        let e = { _from: vv.pos, _to: cluster[toVertexPos].pos,
                  name: v.name + '->' + toVertex.name };
        r.edges.push(e);
      }
      if (rand.nextUnitInterval() > 1.0 - extDegree) {
        let toCluster;
        do {
          toCluster = rand.next() % clusters.length;
        } while (toCluster === i);
        let toVertexPos = rand.next() % clusters[toCluster].length;
        let toVertex = r.vertices[clusters[toCluster][toVertexPos].pos];
        let e = { _from: vv.pos, _to: clusters[toCluster][toVertexPos].pos,
                  name: v.name + '->' + toVertex.name };
        r.edges.push(e);
      }
    }
  }

  return r;
}

// In-memory simulation of graph traversals:

function makeGraphIndex(graph) {
  if (graph.table === undefined ||
      graph.fromTable === undefined || graph.toTable === undefined) {
    graph.table = {};
    graph.fromTable = {};
    graph.toTable = {};
    for (let i = 0; i < graph.vertices.length; ++i) {
      graph.table[graph.vertices[i]._id] = graph.vertices[i];
      graph.fromTable[graph.vertices[i]._id] = [];
      graph.toTable[graph.vertices[i]._id] = [];
    }
    for (let i = 0; i < graph.edges.length; ++i) {
      let a = graph.fromTable[graph.edges[i]._from];
      a.push(i);
      a = graph.toTable[graph.edges[i]._to];
      a.push(i);
    }
  }
}

function produceResult(schreier, minDepth, mode) {
  let r = { res: [], depths: [] };
  for (let i = 0; i < schreier.length; ++i) {
    if (schreier[i].depth < minDepth) {
      continue;
    }
    if (mode === 'vertex') {
      r.res.push(schreier[i].vertex);
    } else if (mode === 'edge') {
      r.res.push({vertex: schreier[i].vertex, edge: schreier[i].edge});
    } else {
      let pos = i;
      let p = {vertices: [], edges: []};
      while (pos !== null) {
        p.vertices.push(schreier[pos].vertex);
        p.edges.push(schreier[pos].edge);
        pos = schreier[pos].pred;
      }
      p.edges.pop();
      p.vertices = p.vertices.reverse();
      p.edges = p.edges.reverse();
      r.res.push(p);
    }
    r.depths.push(schreier[i].depth);
  }
  return r;
}

function simulateBreadthFirstSearch(graph, startVertexId, minDepth, maxDepth,
                                    uniqueness, mode, dir) {
  // <graph> is an object with attributes 'vertices' and 'edges', which
  // simply contain the list of vertices (with their _id attributes)
  // and edges (with _id, _from and _to attributes, all strings).
  // <startVertexId> is the id of the start vertex as a string,
  // <minDepth> (>= 0) and <maxDepth> (>= <minDepth>) are what they
  // say, <uniqueness> can be 'none', 'edgePath', 'vertexPath' or
  // 'vertexGlobal', and <mode> can be 'vertex', 'edge' or 'path', which
  // produces output containing the last vertex, the last vertex and
  // edge, or the complete path is. <dir> can be 'OUT' or 'IN' or 'ANY'
  // for the direction of travel. The result is an object with an array
  // of results (according to <mode>) in the 'res' component as well as
  // an array of indices indicating where the depths start.

  // First create index tables:
  makeGraphIndex(graph);

  // Now start the breadth first search:
  let schreier = [{vertex: graph.table[startVertexId], edge: null,
                   depth: 0, pred: null}];
  let pos = 0;
  let used = {};
  used[startVertexId] = true;

  while (pos < schreier.length) {
    if (schreier[pos].depth >= maxDepth) {
      break;
    }

    let id = schreier[pos].vertex._id;
    let outEdges = graph.fromTable[id];
    let inEdges = graph.toTable[id];
    let done = {};

    let doStep = (edge, vertex) => {
      let useThisEdge = true;
      // add more checks here depending on uniqueness mode
      if (uniqueness === 'edgePath') {
        let p = pos;
        while (p !== 0) {
          if (schreier[p].edge._id === edge._id) {
            useThisEdge = false;
            break;
          }
          p = schreier[p].pred;
        }
      } else if (uniqueness === 'vertexPath') {
        let p = pos;
        while (p !== null) {
          if (schreier[p].vertex._id === vertex._id) {
            useThisEdge = false;
            break;
          }
          p = schreier[p].pred;
        }
      } else if (uniqueness === 'vertexGlobal') {
        if (used[vertex._id] === true) {
          useThisEdge = false;
        }
        used[vertex._id] = true;
      }
      if (useThisEdge) {
        schreier.push({vertex: vertex, edge: edge,
                       depth: schreier[pos].depth + 1, pred: pos});
      }
    };

    if (dir === 'OUT' || dir === 'ANY') {
      for (let i = 0; i < outEdges.length; ++i) {
        let edge = graph.edges[outEdges[i]];
        doStep(edge, graph.table[edge._to]);
        done[edge._id] = true;
      }
    }
    if (dir === 'IN' || dir === 'ANY') {
      for (let i = 0; i < inEdges.length; ++i) {
        let edge = graph.edges[inEdges[i]];
        if (done[edge._id] === true) {
          continue;
        }
        doStep(edge, graph.table[edge._from]);
      }
    }
    ++pos;
  }

  // Now produce the result:
  return produceResult(schreier, minDepth, mode);
}

function simulateDepthFirstSearch(graph, startVertexId, minDepth, maxDepth,
                                  uniqueness, mode, dir) {
  // <graph> is an object with attributes 'vertices' and 'edges', which
  // simply contain the list of vertices (with their _id attributes)
  // and edges (with _id, _from and _to attributes, all strings).
  // <startVertexId> is the id of the start vertex as a string,
  // <minDepth> (>= 0) and <maxDepth> (>= <minDepth>) are what they
  // say, <uniqueness> can be 'none', 'edgePath', 'vertexPath',
  // and <mode> can be 'vertex', 'edge' or 'path', which
  // produces output containing the last vertex, the last vertex and
  // edge, or the complete path is. <dir> can be 'OUT' or 'IN' or 'ANY'
  // for the direction of travel. The result is an object with an array
  // of results (according to <mode>) in the 'res' component as well as
  // an array of indices indicating where the depths start.

  // First create index tables:
  makeGraphIndex(graph);

  // Now start the depth first search:
  let schreier = [{vertex: graph.table[startVertexId], edge: null,
                   depth: 0, pred: null}];

  function doRecurse(pos) {
    if (schreier[pos].depth >= maxDepth) {
      return;
    }

    let id = schreier[pos].vertex._id;
    let outEdges = graph.fromTable[id];
    let inEdges = graph.toTable[id];
    let done = {};

    let doStep = (edge, vertex) => {
      let useThisEdge = true;
      // add more checks here depending on uniqueness mode
      if (uniqueness === 'edgePath') {
        let p = pos;
        while (p !== 0) {
          if (schreier[p].edge._id === edge._id) {
            useThisEdge = false;
            break;
          }
          p = schreier[p].pred;
        }
      } else if (uniqueness === 'vertexPath') {
        let p = pos;
        while (p !== null) {
          if (schreier[p].vertex._id === vertex._id) {
            useThisEdge = false;
            break;
          }
          p = schreier[p].pred;
        }
      }
      if (useThisEdge) {
        schreier.push({vertex: vertex, edge: edge,
                       depth: schreier[pos].depth + 1, pred: pos});
        doRecurse(schreier.length - 1);
      }
    };

    if (dir === 'OUT' || dir === 'ANY') {
      for (let i = 0; i < outEdges.length; ++i) {
        let edge = graph.edges[outEdges[i]];
        doStep(edge, graph.table[edge._to]);
        done[edge._id] = true;
      }
    }
    if (dir === 'IN' || dir === 'ANY') {
      for (let i = 0; i < inEdges.length; ++i) {
        let edge = graph.edges[inEdges[i]];
        if (done[edge._id] === true) {
          continue;
        }
        doStep(edge, graph.table[edge._from]);
      }
    }
  }

  // Fire off the computation:
  doRecurse(0);

  // Now produce the result:
  return produceResult(schreier, minDepth, mode);
}

function pathCompare(a, b) {
  // Establishes a total order on the set of all paths of the form:
  //   { vertices: [...], edges: [...] }
  // where there is one more vertex than edges, and all vertices and
  // edges each have an _id attribute uniquely identifying it.
  if (a.vertices.length < b.vertices.length) {
    return -1;
  } else if (a.vertices.length > b.vertices.length) {
    return 1;
  } else {
    let i = 0;
    while (true) {
      // Will be left by return if i reaches a.vertices.length - 1 at the
      // latest.
      if (a.vertices[i]._id < b.vertices[i]._id) {
        return -1;
      } else if (a.vertices[i]._id > b.vertices[i]._id) {
        return 1;
      } else {
        if (i === a.vertices.length - 1) {
          return 0;   // all is equal
        }
        if (a.edges[i]._id < b.edges[i]._id) {
          return -1;
        } else if (a.edges[i]._id > b.edges[i]._id) {
          return 1;
        }
        // A tie so far, move on.
      }
      i += 1;
    }
  }
}

function checkBFSResult(pattern, toCheck) {
  // This tries to check a BFS result against a given pattern. Some
  // fuzziness is needed since AQL does not promise the order in which
  // edges are followed from a vertex during the traversal. The mode
  // 'vertex', 'edge' or 'path' is detected automatically and refers to
  // the three possibilities of the query:
  //  (1) FOR v IN 0..5 ... OPTIONS {'bfs': true} RETURN v
  //  (2) FOR v, e IN 0..5 ... OPTIONS {'bfs': true} RETURN {vertex: v, edge: e}
  //  (3) FOR v, e, p IN 0..5 ... OPTIONS {'bfs': true} RETURN p
  // The pattern is of the form
  //   { 'res': [...], 'depths': [...] }
  // with two arrays of equal length as returned by simulateBreadthFirstSearch.
  // Further filterings may have been applied to the results.
  // The following checks are done:
  //  (1) Number of results are equal
  //  (2) The set of vertex IDs in each depth is the same in both results
  //  (3) The set of edge IDs in each depth is the same in both results
  // This function returns an object of the form:
  //   res = { 'error': true/false, 'errorMessage': '' }
  // where errorMessage is set iff error is true. This can be used as in:
  //   assertFalse(res.error, res.errorMessage)
  if (pattern.res.length !== toCheck.length) {
    return { error: true, errorMessage: 'Number of results does not match' };
  }
  if (pattern.res.length === 0) {
    return { error: false };
  }
  let guck = pattern.res[0];
  if (typeof guck !== 'object') {
    return { error: true, errorMessage: 'Cannot recognize mode' };
  }
  let mode;
  if (guck.hasOwnProperty('vertices') && guck.hasOwnProperty('edges')) {
    mode = 'path';
  } else if (guck.hasOwnProperty('vertex') && guck.hasOwnProperty('edge')) {
    mode = 'edge';
  } else {
    mode = 'vertex';
  }

  // Not let the show start, first find the depth levels:
  let newDepths = [];
  let depth = null;
  for (let i = 0; i < pattern.depths.length; ++i) {
    if (pattern.depths[i] !== depth) {
      if (depth !== null && pattern.depths[i] < depth) {
        return { error: true,
                 errorMessage: 'Depths are decreasing' };
      }
      depth = pattern.depths[i];
      newDepths.push(i);
    }
  }
  newDepths.push(pattern.depths.length);

  // Now check one depth after another:
  for (let i = 0; i < newDepths.length - 1; ++i) {
    if (mode === 'vertex') {
      let tab = {};
      for (let j = newDepths[i]; j < newDepths[i+1]; ++j) {
        tab[pattern.res[j]._id] = true;
      }
      for (let j = newDepths[i]; j < newDepths[i+1]; ++j) {
        if (tab[toCheck[j]._id] !== true) {
          return { error: true, errorMessage: 'Ids in depth ' +
                   pattern.depths[j] + ' do not match, "' +
                   toCheck[j]._id + '" in toCheck is not in pattern' };
        }
      }
    } else if (mode === 'edge') {
      let vTab = {};
      let eTab = {};
      for (let j = newDepths[i]; j < newDepths[i+1]; ++j) {
        vTab[pattern.res[j].vertex._id] = true;
        if (pattern.res[j].edge !== null) {
          eTab[pattern.res[j].edge._id] = true;
        }
      }
      for (let j = newDepths[i]; j < newDepths[i+1]; ++j) {
        if (vTab[toCheck[j].vertex._id] !== true) {
          return { error: true, errorMessage: 'Vertex ids in depth ' +
                   pattern.depths[j] + ' do not match, "' +
                   toCheck[j].vertex._id + '" in toCheck is not in pattern' };
        }
        if (toCheck[j].edge !== null &&
            eTab[toCheck[j].edge._id] !== true) {
          return { error: true, errorMessage: 'Edge ids in depth ' +
                   pattern.depths[j] + ' do not match, "' +
                   toCheck[j].edge._id + '" in toCheck is not in pattern' };
        }
      }
    } else {   // mode === 'path'
      let colA = [];
      let colB = [];
      for (let j = newDepths[i]; j < newDepths[i+1]; ++j) {
        colA.push(pattern.res[j]);
        colB.push(toCheck[j]);
      }
      colA.sort(pathCompare);
      colB.sort(pathCompare);
      for (let j = 0; j < colA.length; ++j) {
        if (pathCompare(colA[j], colB[j]) !== 0) {
          return { error: true, errorMessage: 'Path sets in depth ' +
                   pattern.depths[newDepths[i]] + ' do not match, here is a ' +
                   'sample: ' + JSON.stringify(colA) + ' (pattern) as opposed '+
                   ' to ' + JSON.stringify(colB) + ' (toCheck)' };
        }
      }
    }
  }

  // All done, all is fine:
  return { error: false };
}

function checkDFSResult(pattern, toCheck) {
  // This tries to check a DFS result against a given pattern. Some
  // fuzziness is needed since AQL does not promise the order in which
  // edges are followed from a vertex during the traversal. The only mode
  // is 'path' and refers to the following form of query:
  //   FOR v, e, p IN 0..5 ... RETURN p
  // The pattern is of the form
  //   { 'res': [...], 'depths': [...] }
  // with two arrays of equal length as returned by simulateDepthFirstSearch.
  // Further filterings may have been applied to the results.
  // The following checks are done:
  //  (1) Number of results are equal
  //  (2) The set of paths is the same in both results
  // This function returns an object of the form:
  //   res = { 'error': true/false, 'errorMessage': '' }
  // where errorMessage is set iff error is true. This can be used as in:
  //   assertFalse(res.error, res.errorMessage)
  if (pattern.res.length !== toCheck.length) {
    return { error: true, errorMessage: 'Number of results does not match' };
  }
  if (pattern.res.length === 0) {
    return { error: false };
  }

  let colA = pattern.res.map(i => i);
  let colB = toCheck.map(i => i);
  colA.sort(pathCompare);
  colB.sort(pathCompare);
  for (let j = 0; j < colA.length; ++j) {
    if (pathCompare(colA[j], colB[j]) !== 0) {
      return { error: true, errorMessage: 'Path sets do not match, here is a ' +
               'sample: ' + JSON.stringify(colA) + ' (pattern) as opposed '+
               ' to ' + JSON.stringify(colB) + ' (toCheck)' };
    }
  }

  // All done, all is fine:
  return { error: false };
}

function storeGraph(r, Vname, Ename, Gname) {
  // r a graph made by makeTree or makeClusteredGraph, Vname a string for the
  // name of the vertex collection, Ename a string for the name of the edge
  // collection, Gname is the name of the named graph, returns an object
  // with two attributes "graph" with the general graph object and "data"
  // with the object to be used by simulateBreadthFirstSearch.
  let db = require('internal').db;
  let g = require('@arangodb/general-graph');
  db._drop(Vname);
  db._drop(Ename);
  let V = db._create(Vname);
  let E = db._createEdgeCollection(Ename);
  let vv = [];
  for (let i = 0; i < r.vertices.length; ++i) {
    vv.push(V.insert(r.vertices[i]));
  }
  for (let i = 0; i < r.edges.length; ++i) {
    let e = r.edges[i];
    e._from = vv[e._from]._id;
    e._to = vv[e._to]._id;
    E.insert(e);
  }
  try {
    g._drop(Gname);
  } catch (err) {
  }
  let graph = g._create(Gname);
  graph._extendEdgeDefinitions(g._relation(Ename, [Vname], [Vname]));
  return { data: { vertices: V.toArray(), edges: E.toArray() },
           graph };
}

let runTraversalRestrictEdgeCollectionTests = function (vn, en, gn, checkOptimizerRule) {
  let fillGraph = function () {
    let keys = [];
    let v = db._collection(vn);
    for (let i = 0; i < 10; ++i) {
      keys.push(v.insert({ value: i })._key);
    }

    let e1 = db._collection(en + "1");
    e1.insert({ _from: vn + "/" + keys[0], _to: vn + "/" + keys[1] });
    e1.insert({ _from: vn + "/" + keys[1], _to: vn + "/" + keys[2] });
    e1.insert({ _from: vn + "/" + keys[2], _to: vn + "/" + keys[3] });
    e1.insert({ _from: vn + "/" + keys[3], _to: vn + "/" + keys[4] });
    e1.insert({ _from: vn + "/" + keys[4], _to: vn + "/" + keys[5] });

    let e2 = db._collection(en + "2");
    e2.insert({ _from: vn + "/" + keys[0], _to: vn + "/" + keys[6] });
    e2.insert({ _from: vn + "/" + keys[1], _to: vn + "/" + keys[7] });
    e2.insert({ _from: vn + "/" + keys[7], _to: vn + "/" + keys[8] });
    e2.insert({ _from: vn + "/" + keys[8], _to: vn + "/" + keys[9] });

    return keys;
  };

  let keys = fillGraph();

  let queries = [
    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" ${en}1 SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}1 SORT v._id RETURN DISTINCT v._id`, [ 0, 1 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}1 SORT v._id RETURN DISTINCT v._id`, [ 1 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}1 SORT v._id RETURN DISTINCT v._id`, [ 1, 2 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}1 SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 0, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],

    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 0, 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 6, 7 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3, 6, 7, 8 ] ],

    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" SORT v._id RETURN DISTINCT v._id`, [ 0, 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" SORT v._id RETURN DISTINCT v._id`, [ 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 6, 7 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3, 6, 7, 8 ] ],

    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" ${en}1 OPTIONS { edgeCollections: "${en}1" } SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}1 OPTIONS { edgeCollections: "${en}1" } SORT v._id RETURN DISTINCT v._id`, [ 0, 1 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}1 OPTIONS { edgeCollections: "${en}1" } SORT v._id RETURN DISTINCT v._id`, [ 1 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}1 OPTIONS { edgeCollections: "${en}1" } SORT v._id RETURN DISTINCT v._id`, [ 1, 2 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}1 OPTIONS { edgeCollections: "${en}1" } SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}2 OPTIONs { edgeCollections: "${en}2" } SORT v._id RETURN DISTINCT v._id`, [ 0, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}2 OPTIONS { edgeCollections: "${en}2" } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}2 OPTIONS { edgeCollections: "${en}2" } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}2 OPTIONS { edgeCollections: "${en}2" } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],

    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0, 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 6, 7 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3, 6, 7, 8 ] ],

    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 0, 1 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 1 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" ${en}1, ${en}2 OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],

    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 0, 1 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 1 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 6 ] ],

    [ `WITH ${vn} FOR v IN 0..0 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0 ] ],
    [ `WITH ${vn} FOR v IN 0..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 0, 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..1 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 6 ] ],
    [ `WITH ${vn} FOR v IN 1..2 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 6, 7 ] ],
    [ `WITH ${vn} FOR v IN 1..3 OUTBOUND "${vn}/${keys[0]}" GRAPH "${gn}" OPTIONS { edgeCollections: [ "${en}1", "${en}2" ] } SORT v._id RETURN DISTINCT v._id`, [ 1, 2, 3, 6, 7, 8 ] ],
  ];

  queries.forEach(function(q) {
    if (checkOptimizerRule !== undefined) {
      assertNotEqual(-1, AQL_EXPLAIN(q[0]).plan.rules.indexOf(checkOptimizerRule));
    }
    const actual = db._query(q[0]).toArray();
    let expected = [];
    q[1].forEach(function(e) {
      expected.push(vn + "/" + keys[e]);
    });
    assertEqual(actual, expected, q);
  });
};

exports.makeTree = makeTree;
exports.PoorMansRandom = PoorMansRandom;
exports.makeClusteredGraph = makeClusteredGraph;
exports.simulateBreadthFirstSearch = simulateBreadthFirstSearch;
exports.simulateDepthFirstSearch = simulateDepthFirstSearch;
exports.checkBFSResult = checkBFSResult;
exports.checkDFSResult = checkDFSResult;
exports.storeGraph = storeGraph;
exports.runTraversalRestrictEdgeCollectionTests = runTraversalRestrictEdgeCollectionTests;
