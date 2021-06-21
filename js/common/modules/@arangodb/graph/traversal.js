/* jshint strict: false, unused: true */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Traversal "classes"
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2013 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Michael Hackstein
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var generalGraph = require('@arangodb/general-graph');
var arangodb = require('@arangodb');
var BinaryHeap = require('@arangodb/heap').BinaryHeap;
var ArangoError = arangodb.ArangoError;

var db = arangodb.db;

var ArangoTraverser;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clone any object
// //////////////////////////////////////////////////////////////////////////////

function clone (obj) {
  if (obj === null || typeof obj !== 'object') {
    return obj;
  }

  var copy;
  if (Array.isArray(obj)) {
    copy = [];
    obj.forEach(function (i) {
      copy.push(clone(i));
    });
  } else if (obj instanceof Object) {
    copy = { };
    Object.keys(obj).forEach(function (k) {
      copy[k] = clone(obj[k]);
    });
  }

  return copy;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test if object is empty
// //////////////////////////////////////////////////////////////////////////////

function isEmpty (obj) {
  for (var key in obj) {
    if (obj.hasOwnProperty(key)) {
      return false;
    }
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief traversal abortion exception
// //////////////////////////////////////////////////////////////////////////////

var abortedException = function (message, options) {
  'use strict';
  this.message = message || 'traversal intentionally aborted by user';
  this.options = options || { };
  this._intentionallyAborted = true;
};

abortedException.prototype = new Error();

// //////////////////////////////////////////////////////////////////////////////
// / @brief default ArangoCollection datasource
// /
// / This is a factory function that creates a datasource that operates on the
// / specified edge collection. The vertices and edges are the documents in the
// / corresponding collections.
// //////////////////////////////////////////////////////////////////////////////

function collectionDatasourceFactory (edgeCollection) {
  var c = edgeCollection;
  if (typeof c === 'string') {
    c = db._collection(c);
  }

  // we can call the "fast" version of some edge functions if we are
  // running server-side and are not a coordinator
  var useBuiltIn = require('internal').isArangod();
  if (useBuiltIn && require('@arangodb/cluster').isCoordinator()) {
    useBuiltIn = false;
  }

  return {
    edgeCollection: c,
    useBuiltIn: useBuiltIn,

    getVertexId: function (vertex) {
      return vertex._id;
    },

    getPeerVertex: function (edge, vertex) {
      if (edge._from === vertex._id) {
        return db._document(edge._to);
      }

      if (edge._to === vertex._id) {
        return db._document(edge._from);
      }

      return null;
    },

    getInVertex: function (edge) {
      return db._document(edge._to);
    },

    getOutVertex: function (edge) {
      return db._document(edge._from);
    },

    getEdgeId: function (edge) {
      return edge._id;
    },

    getEdgeFrom: function (edge) {
      return edge._from;
    },

    getEdgeTo: function (edge) {
      return edge._to;
    },

    getLabel: function (edge) {
      return edge.$label;
    },

    getAllEdges: function (vertex) {
      if (this.useBuiltIn) {
        return this.edgeCollection.EDGES(vertex._id);
      }
      return this.edgeCollection.edges(vertex._id);
    },

    getInEdges: function (vertex) {
      if (this.useBuiltIn) {
        return this.edgeCollection.INEDGES(vertex._id);
      }
      return this.edgeCollection.inEdges(vertex._id);
    },

    getOutEdges: function (vertex) {
      if (this.useBuiltIn) {
        return this.edgeCollection.OUTEDGES(vertex._id);
      }
      return this.edgeCollection.outEdges(vertex._id);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief general graph datasource
// /
// / This is a factory function that creates a datasource that operates on the
// / specified general graph. The vertices and edges are delivered by the
// / the general-graph module.
// //////////////////////////////////////////////////////////////////////////////

function generalGraphDatasourceFactory (graph) {
  var g = graph;
  if (typeof g === 'string') {
    g = generalGraph._graph(g);
  }

  return {
    graph: g,

    getVertexId: function (vertex) {
      return vertex._id;
    },

    getPeerVertex: function (edge, vertex) {
      if (edge._from === vertex._id) {
        return db._document(edge._to);
      }

      if (edge._to === vertex._id) {
        return db._document(edge._from);
      }

      return null;
    },

    getInVertex: function (edge) {
      return db._document(edge._to);
    },

    getOutVertex: function (edge) {
      return db._document(edge._from);
    },

    getEdgeId: function (edge) {
      return edge._id;
    },

    getEdgeFrom: function (edge) {
      return edge._from;
    },

    getEdgeTo: function (edge) {
      return edge._to;
    },

    getLabel: function (edge) {
      return edge.$label;
    },

    getAllEdges: function (vertex) {
      return this.graph._EDGES(vertex._id);
    },

    getInEdges: function (vertex) {
      return this.graph._INEDGES(vertex._id);
    },

    getOutEdges: function (vertex) {
      return this.graph._OUTEDGES(vertex._id);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief default outbound expander function
// //////////////////////////////////////////////////////////////////////////////

function outboundExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [];
  var outEdges = datasource.getOutEdges(vertex);
  var edgeIterator;

  if (outEdges.length > 1 && config.sort) {
    outEdges.sort(config.sort);
  }

  if (config.buildVertices) {
    if (!config.expandFilter) {
      edgeIterator = function (edge) {
        try {
          var v = datasource.getInVertex(edge);
          connections.push({ edge: edge, vertex: v });
        } catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    } else {
      edgeIterator = function (edge) {
        try {
          var v = datasource.getInVertex(edge);
          if (config.expandFilter(config, v, edge, path)) {
            connections.push({ edge: edge, vertex: v });
          }
        } catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    }
  } else {
    if (!config.expandFilter) {
      edgeIterator = function (edge) {
        var id = datasource.getEdgeTo(edge);
        var v = { _id: id, _key: id.substr(id.indexOf('/') + 1)};
        connections.push({ edge: edge, vertex: v });
      };
    } else {
      edgeIterator = function (edge) {
        var id = datasource.getEdgeTo(edge);
        var v = { _id: id, _key: id.substr(id.indexOf('/') + 1)};
        if (config.expandFilter(config, v, edge, path)) {
          connections.push({ edge: edge, vertex: v });
        }
      };
    }
  }
  outEdges.forEach(edgeIterator);
  return connections;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief default inbound expander function
// //////////////////////////////////////////////////////////////////////////////

function inboundExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [];

  var inEdges = datasource.getInEdges(vertex);

  if (inEdges.length > 1 && config.sort) {
    inEdges.sort(config.sort);
  }
  var edgeIterator;

  if (config.buildVertices) {
    if (!config.expandFilter) {
      edgeIterator = function (edge) {
        try {
          var v = datasource.getOutVertex(edge);
          connections.push({ edge: edge, vertex: v });
        } catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    } else {
      edgeIterator = function (edge) {
        try {
          var v = datasource.getOutVertex(edge);
          if (config.expandFilter(config, v, edge, path)) {
            connections.push({ edge: edge, vertex: v });
          }
        } catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    }
  } else {
    if (!config.expandFilter) {
      edgeIterator = function (edge) {
        var id = datasource.getEdgeFrom(edge);
        var v = { _id: id, _key: id.substr(id.indexOf('/') + 1)};
        connections.push({ edge: edge, vertex: v });
      };
    } else {
      edgeIterator = function (edge) {
        var id = datasource.getEdgeFrom(edge);
        var v = { _id: id, _key: id.substr(id.indexOf('/') + 1)};
        if (config.expandFilter(config, v, edge, path)) {
          connections.push({ edge: edge, vertex: v });
        }
      };
    }
  }
  inEdges.forEach(edgeIterator);

  return connections;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief default "any" expander function
// //////////////////////////////////////////////////////////////////////////////

function anyExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [];
  var edges = datasource.getAllEdges(vertex);

  if (edges.length > 1 && config.sort) {
    edges.sort(config.sort);
  }

  var edgeIterator;
  if (config.buildVertices) {
    if (!config.expandFilter) {
      edgeIterator = function (edge) {
        try {
          var v = datasource.getPeerVertex(edge, vertex);
          connections.push({ edge: edge, vertex: v });
        } catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    } else {
      edgeIterator = function (edge) {
        try {
          var v = datasource.getPeerVertex(edge, vertex);
          if (config.expandFilter(config, v, edge, path)) {
            connections.push({ edge: edge, vertex: v });
          }
        } catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    }
  } else {
    if (!config.expandFilter) {
      edgeIterator = function (edge) {
        var id = datasource.getEdgeFrom(edge);
        if (id === vertex._id) {
          id = datasource.getEdgeTo(edge);
        }
        var v = { _id: id, _key: id.substr(id.indexOf('/') + 1)};
        connections.push({ edge: edge, vertex: v });
      };
    } else {
      edgeIterator = function (edge) {
        var id = datasource.getEdgeFrom(edge);
        if (id === vertex._id) {
          id = datasource.getEdgeTo(edge);
        }
        var v = { _id: id, _key: id.substr(id.indexOf('/') + 1)};
        if (config.expandFilter(config, v, edge, path)) {
          connections.push({ edge: edge, vertex: v });
        }
      };
    }
  }
  edges.forEach(edgeIterator);
  return connections;
}

// /////////////////////////////////////////////////////////////////////////////////////////
// / @brief expands all outbound edges labeled with at least one label in config.labels
// /////////////////////////////////////////////////////////////////////////////////////////

function expandOutEdgesWithLabels (config, vertex) {
  var datasource = config.datasource;
  var result = [];
  var i;

  if (!Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }

  var edgesList = datasource.getOutEdges(vertex);

  if (edgesList !== undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      var edge = edgesList[i];
      var label = datasource.getLabel(edge);

      if (config.labels.indexOf(label) >= 0) {
        result.push({ edge: edge, vertex: datasource.getInVertex(edge) });
      }
    }
  }

  return result;
}

// /////////////////////////////////////////////////////////////////////////////////////////
// / @brief expands all inbound edges labeled with at least one label in config.labels
// /////////////////////////////////////////////////////////////////////////////////////////

function expandInEdgesWithLabels (config, vertex) {
  var datasource = config.datasource;
  var result = [];
  var i;

  if (!Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }

  var edgesList = config.datasource.getInEdges(vertex);

  if (edgesList !== undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      var edge = edgesList[i];
      var label = datasource.getLabel(edge);

      if (config.labels.indexOf(label) >= 0) {
        result.push({ edge: edge, vertex: datasource.getOutVertex(edge) });
      }
    }
  }

  return result;
}

// /////////////////////////////////////////////////////////////////////////////////////////
// / @brief expands all edges labeled with at least one label in config.labels
// /////////////////////////////////////////////////////////////////////////////////////////

function expandEdgesWithLabels (config, vertex) {
  var datasource = config.datasource;
  var result = [];
  var i;

  if (!Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }

  var edgesList = config.datasource.getAllEdges(vertex);

  if (edgesList !== undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      var edge = edgesList[i];
      var label = datasource.getLabel(edge);

      if (config.labels.indexOf(label) >= 0) {
        result.push({ edge: edge, vertex: datasource.getPeerVertex(edge, vertex) });
      }
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief default visitor that just tracks every visit
// //////////////////////////////////////////////////////////////////////////////

function trackingVisitor (config, result, vertex, path) {
  if (!result || !result.visited) {
    return;
  }

  if (result.visited.vertices) {
    result.visited.vertices.push(clone(vertex));
  }

  if (result.visited.paths) {
    result.visited.paths.push(clone(path));
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief a visitor that counts the number of nodes visited
// //////////////////////////////////////////////////////////////////////////////

function countingVisitor (config, result) {
  if (!result) {
    return;
  }

  if (result.hasOwnProperty('count')) {
    ++result.count;
  } else {
    result.count = 1;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief a visitor that does nothing - can be used to quickly traverse a
// / graph, e.g. for performance comparisons etc.
// //////////////////////////////////////////////////////////////////////////////

function doNothingVisitor () {
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief default filter to visit & expand all vertices
// //////////////////////////////////////////////////////////////////////////////

function visitAllFilter () {
  return '';
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief filter to visit & expand all vertices up to a given depth
// //////////////////////////////////////////////////////////////////////////////

function maxDepthFilter (config, vertex, path) {
  if (path && path.vertices && path.vertices.length > config.maxDepth) {
    return ArangoTraverser.PRUNE;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief exclude all vertices up to a given depth
// //////////////////////////////////////////////////////////////////////////////

function minDepthFilter (config, vertex, path) {
  if (path && path.vertices && path.vertices.length <= config.minDepth) {
    return ArangoTraverser.EXCLUDE;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief include all vertices matching one of the given attribute sets
// //////////////////////////////////////////////////////////////////////////////

function includeMatchingAttributesFilter (config, vertex) {
  if (!Array.isArray(config.matchingAttributes)) {
    config.matchingAttributes = [config.matchingAttributes];
  }

  var include = false;

  config.matchingAttributes.forEach(function (example) {
    var count = 0;
    var keys = Object.keys(example);

    keys.forEach(function (key) {
      if (vertex[key] && vertex[key] === example[key]) {
        count++;
      }
    });

    if (count > 0 && count === keys.length) {
      include = true;
    }
  });

  var result;

  if (!include) {
    result = 'exclude';
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief combine an array of filters
// //////////////////////////////////////////////////////////////////////////////

function combineFilters (filters, config, vertex, path) {
  var result = [];

  filters.forEach(function (f) {
    var tmp = f(config, vertex, path);

    if (!Array.isArray(tmp)) {
      tmp = [ tmp ];
    }

    result = result.concat(tmp);
  });

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief parse a filter result
// //////////////////////////////////////////////////////////////////////////////

function parseFilterResult (args) {
  var result = {
    visit: true,
    expand: true
  };

  function processArgument (arg) {
    if (arg === undefined || arg === null) {
      return;
    }

    var finish = false;

    if (typeof (arg) === 'string') {
      if (arg === ArangoTraverser.EXCLUDE) {
        result.visit = false;
        finish = true;
      } else if (arg === ArangoTraverser.PRUNE) {
        result.expand = false;
        finish = true;
      } else if (arg === '') {
        finish = true;
      }
    } else if (Array.isArray(arg)) {
      var i;
      for (i = 0; i < arg.length; ++i) {
        processArgument(arg[i]);
      }
      finish = true;
    }

    if (finish) {
      return;
    }

    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_FILTER_RESULT.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_FILTER_RESULT.message;
    throw err;
  }

  processArgument(args);

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply the uniqueness checks
// //////////////////////////////////////////////////////////////////////////////

function checkUniqueness (config, visited, vertex, edge) {
  var uniqueness = config.uniqueness;
  var datasource = config.datasource;
  var id;

  if (uniqueness.vertices !== ArangoTraverser.UNIQUE_NONE) {
    id = datasource.getVertexId(vertex);

    if (visited.vertices[id] === true) {
      return false;
    }

    visited.vertices[id] = true;
  }

  if (edge !== null && uniqueness.edges !== ArangoTraverser.UNIQUE_NONE) {
    id = datasource.getEdgeId(edge);

    if (visited.edges[id] === true) {
      return false;
    }

    visited.edges[id] = true;
  }

  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief check if we must process items in reverse order
// //////////////////////////////////////////////////////////////////////////////

function checkReverse (config) {
  var result = false;

  if (config.order === ArangoTraverser.POST_ORDER) {
    // post order
    if (config.itemOrder === ArangoTraverser.FORWARD) {
      result = true;
    }
  } else if (config.order === ArangoTraverser.PRE_ORDER ||
    config.order === ArangoTraverser.PRE_ORDER_EXPANDER) {
    // pre order
    if (config.itemOrder === ArangoTraverser.BACKWARD &&
      config.strategy === ArangoTraverser.BREADTH_FIRST) {
      result = true;
    } else if (config.itemOrder === ArangoTraverser.FORWARD &&
      config.strategy === ArangoTraverser.DEPTH_FIRST) {
      result = true;
    }
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief implementation details for breadth-first strategy
// //////////////////////////////////////////////////////////////////////////////

function breadthFirstSearch () {
  return {
    requiresEndVertex: function () {
      return false;
    },

    getPathItems: function (id, items) {
      var visited = { };
      var ignore = items.length - 1;

      items.forEach(function (item, i) {
        if (i !== ignore) {
          visited[id(item)] = true;
        }
      });

      return visited;
    },

    createPath: function (items, idx) {
      var path = { edges: [], vertices: [] };
      var pathItem = items[idx];

      while (true) {
        if (pathItem.edge !== null) {
          path.edges.unshift(pathItem.edge);
        }
        path.vertices.unshift(pathItem.vertex);
        idx = pathItem.parentIndex;
        if (idx < 0) {
          break;
        }
        pathItem = items[idx];
      }

      return path;
    },

    run: function (config, result, startVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;
      var toVisit = [ { edge: null, vertex: startVertex, parentIndex: -1 } ];
      var visited = { edges: { }, vertices: { } };

      var index = 0;
      var step = 1;
      var reverse = checkReverse(config);

      while ((step === 1 && index < toVisit.length) ||
        (step === -1 && index >= 0)) {
        var current = toVisit[index];
        var vertex = current.vertex;
        var edge = current.edge;
        var path;

        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        if (current.visit === null || current.visit === undefined) {
          current.visit = false;

          path = this.createPath(toVisit, index);

          // first apply uniqueness check
          if (config.uniqueness.vertices === ArangoTraverser.UNIQUE_PATH) {
            visited.vertices = this.getPathItems(config.datasource.getVertexId, path.vertices);
          }
          if (config.uniqueness.edges === ArangoTraverser.UNIQUE_PATH) {
            visited.edges = this.getPathItems(config.datasource.getEdgeId, path.edges);
          }

          if (!checkUniqueness(config, visited, vertex, edge)) {
            if (index < toVisit.length - 1) {
              index += step;
            } else {
              step = -1;
            }
            continue;
          }

          var filterResult = parseFilterResult(config.filter(config, vertex, path));
          if (config.order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
            // preorder
            config.visitor(config, result, vertex, path);
          } else {
            // postorder
            current.visit = filterResult.visit || false;
          }

          if (filterResult.expand) {
            var connected = config.expander(config, vertex, path), i;

            if (reverse) {
              connected.reverse();
            }

            if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) {
              config.visitor(config, result, vertex, path, connected);
            }

            for (i = 0; i < connected.length; ++i) {
              connected[i].parentIndex = index;
              toVisit.push(connected[i]);
            }
          } else if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) {
            config.visitor(config, result, vertex, path, []);
          }

          if (config.order === ArangoTraverser.POST_ORDER) {
            if (index < toVisit.length - 1) {
              index += step;
            } else {
              step = -1;
            }
          }
        } else {
          if (config.order === ArangoTraverser.POST_ORDER && current.visit) {
            path = this.createPath(toVisit, index);
            config.visitor(config, result, vertex, path);
          }
          index += step;
        }
      }
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief implementation details for depth-first strategy
// //////////////////////////////////////////////////////////////////////////////

function depthFirstSearch () {
  return {
    requiresEndVertex: function () {
      return false;
    },

    getPathItems: function (id, items) {
      var visited = { };
      items.forEach(function (item) {
        visited[id(item)] = true;
      });
      return visited;
    },

    run: function (config, result, startVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;
      var toVisit = [ { edge: null, vertex: startVertex, visit: null } ];
      var path = { edges: [], vertices: [] };
      var visited = { edges: { }, vertices: { } };
      var reverse = checkReverse(config);
      var uniqueness = config.uniqueness;
      var haveUniqueness = ((uniqueness.vertices !== ArangoTraverser.UNIQUE_NONE) ||
        (uniqueness.edges !== ArangoTraverser.UNIQUE_NONE));

      while (toVisit.length > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        // peek at the top of the stack
        var current = toVisit[toVisit.length - 1];
        var vertex = current.vertex;
        var edge = current.edge;

        // check if we visit the element for the first time
        if (current.visit === null || current.visit === undefined) {
          current.visit = false;

          if (haveUniqueness) {
            // first apply uniqueness check
            if (uniqueness.vertices === ArangoTraverser.UNIQUE_PATH) {
              visited.vertices = this.getPathItems(config.datasource.getVertexId, path.vertices);
            }

            if (uniqueness.edges === ArangoTraverser.UNIQUE_PATH) {
              visited.edges = this.getPathItems(config.datasource.getEdgeId, path.edges);
            }

            if (!checkUniqueness(config, visited, vertex, edge)) {
              // skip element if not unique
              toVisit.pop();
              continue;
            }
          }

          // push the current element onto the path stack
          if (edge !== null) {
            path.edges.push(edge);
          }

          path.vertices.push(vertex);

          var filterResult = parseFilterResult(config.filter(config, vertex, path));
          if (config.order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
            // preorder visit
            config.visitor(config, result, vertex, path);
          } else {
            // postorder. mark the element visitation flag because we'll got to check it later
            current.visit = filterResult.visit || false;
          }

          // expand the element's children?
          if (filterResult.expand) {
            var connected = config.expander(config, vertex, path), i;

            if (reverse) {
              connected.reverse();
            }

            if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) {
              config.visitor(config, result, vertex, path, connected);
            }

            for (i = 0; i < connected.length; ++i) {
              connected[i].visit = null;
              toVisit.push(connected[i]);
            }
          } else if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) {
            config.visitor(config, result, vertex, path, []);
          }
        } else {
          // we have already seen this element
          if (config.order === ArangoTraverser.POST_ORDER && current.visit) {
            // postorder visitation
            config.visitor(config, result, vertex, path);
          }

          // pop the element from the stack
          toVisit.pop();
          if (path.edges.length > 0) {
            path.edges.pop();
          }
          path.vertices.pop();
        }
      }
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief implementation details for dijkstra shortest path strategy
// //////////////////////////////////////////////////////////////////////////////

function dijkstraSearch () {
  return {
    nodes: { },

    requiresEndVertex: function () {
      return true;
    },

    makeNode: function (vertex) {
      var id = vertex._id;
      if (!this.nodes.hasOwnProperty(id)) {
        this.nodes[id] = { vertex: vertex, dist: Infinity };
      }

      return this.nodes[id];
    },

    vertexList: function (vertex) {
      var result = [];
      while (vertex) {
        result.push(vertex);
        vertex = vertex.parent;
      }
      return result;
    },

    buildPath: function (vertex) {
      var path = { vertices: [ vertex.vertex ], edges: [] };
      var v = vertex;

      while (v.parent) {
        path.vertices.unshift(v.parent.vertex);
        path.edges.unshift(v.parentEdge);
        v = v.parent;
      }
      return path;
    },

    run: function (config, result, startVertex, endVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;

      var heap = new BinaryHeap(function (node) {
        return node.dist;
      });

      var startNode = this.makeNode(startVertex);
      startNode.dist = 0;
      heap.push(startNode);

      while (heap.size() > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        var currentNode = heap.pop();
        var i, n;

        if (currentNode.vertex._id === endVertex._id) {
          var vertices = this.vertexList(currentNode).reverse();

          n = vertices.length;
          for (i = 0; i < n; ++i) {
            if (!vertices[i].hide) {
              config.visitor(config, result, vertices[i].vertex, this.buildPath(vertices[i]));
            }
          }
          return;
        }

        if (currentNode.visited) {
          continue;
        }

        if (currentNode.dist === Infinity) {
          break;
        }

        currentNode.visited = true;

        var path = this.buildPath(currentNode);
        var filterResult = parseFilterResult(config.filter(config, currentNode.vertex, path));

        if (!filterResult.visit) {
          currentNode.hide = true;
        }

        if (!filterResult.expand) {
          continue;
        }

        var dist = currentNode.dist;
        var connected = config.expander(config, currentNode.vertex, path);
        n = connected.length;

        for (i = 0; i < n; ++i) {
          var neighbor = this.makeNode(connected[i].vertex);

          if (neighbor.visited) {
            continue;
          }

          var edge = connected[i].edge;
          var weight = 1;
          if (config.distance) {
            weight = config.distance(config, currentNode.vertex, neighbor.vertex, edge);
          } else if (config.weight) {
            if (typeof edge[config.weight] === 'number') {
              weight = edge[config.weight];
            } else if (config.defaultWeight) {
              weight = config.defaultWeight;
            } else {
              weight = Infinity;
            }
          }

          var alt = dist + weight;
          if (alt < neighbor.dist) {
            neighbor.dist = alt;
            neighbor.parent = currentNode;
            neighbor.parentEdge = edge;
            heap.push(neighbor);
          }
        }
      }
    }
  };
}

function dijkstraSearchMulti () {
  return {
    nodes: { },

    requiresEndVertex: function () {
      return true;
    },

    makeNode: function (vertex) {
      var id = vertex._id;
      if (!this.nodes.hasOwnProperty(id)) {
        this.nodes[id] = { vertex: vertex, dist: Infinity };
      }

      return this.nodes[id];
    },

    vertexList: function (vertex) {
      var result = [];
      while (vertex) {
        result.push(vertex);
        vertex = vertex.parent;
      }
      return result;
    },

    buildPath: function (vertex) {
      var path = { vertices: [ vertex.vertex ], edges: [] };
      var v = vertex;

      while (v.parent) {
        path.vertices.unshift(v.parent.vertex);
        path.edges.unshift(v.parentEdge);
        v = v.parent;
      }
      return path;
    },

    run: function (config, result, startVertex, endVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;

      var heap = new BinaryHeap(function (node) {
        return node.dist;
      });

      var startNode = this.makeNode(startVertex);
      startNode.dist = 0;
      heap.push(startNode);

      while (heap.size() > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        var currentNode = heap.pop();
        var i, n;

        if (endVertex.hasOwnProperty(currentNode.vertex._id)) {
          delete endVertex[currentNode.vertex._id];
          config.visitor(config, result, currentNode, this.buildPath(currentNode));
          if (isEmpty(endVertex)) {
            return;
          }
        }

        if (currentNode.visited) {
          continue;
        }

        if (currentNode.dist === Infinity) {
          break;
        }

        currentNode.visited = true;

        var path = this.buildPath(currentNode);
        var filterResult = parseFilterResult(config.filter(config, currentNode.vertex, path));

        if (!filterResult.visit) {
          currentNode.hide = true;
        }

        if (!filterResult.expand) {
          continue;
        }

        var dist = currentNode.dist;
        var connected = config.expander(config, currentNode.vertex, path);
        n = connected.length;

        for (i = 0; i < n; ++i) {
          var neighbor = this.makeNode(connected[i].vertex);

          if (neighbor.visited) {
            continue;
          }

          var edge = connected[i].edge;
          var weight = 1;
          if (config.distance) {
            weight = config.distance(config, currentNode.vertex, neighbor.vertex, edge);
          } else if (config.weight) {
            if (typeof edge[config.weight] === 'number') {
              weight = edge[config.weight];
            } else if (config.defaultWeight) {
              weight = config.defaultWeight;
            } else {
              weight = Infinity;
            }
          }

          var alt = dist + weight;
          if (alt < neighbor.dist) {
            neighbor.dist = alt;
            neighbor.parent = currentNode;
            neighbor.parentEdge = edge;
            heap.push(neighbor);
          }
        }
      }
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief implementation details for a* shortest path strategy
// //////////////////////////////////////////////////////////////////////////////

function astarSearch () {
  return {
    nodes: { },

    requiresEndVertex: function () {
      return true;
    },

    makeNode: function (vertex) {
      var id = vertex._id;
      if (!this.nodes.hasOwnProperty(id)) {
        this.nodes[id] = { vertex: vertex, f: 0, g: 0, h: 0 };
      }

      return this.nodes[id];
    },

    vertexList: function (vertex) {
      var result = [];
      while (vertex) {
        result.push(vertex);
        vertex = vertex.parent;
      }
      return result;
    },

    buildPath: function (vertex) {
      var path = { vertices: [ vertex.vertex ], edges: [] };
      var v = vertex;

      while (v.parent) {
        path.vertices.unshift(v.parent.vertex);
        path.edges.unshift(v.parentEdge);
        v = v.parent;
      }
      return path;
    },

    run: function (config, result, startVertex, endVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;

      var heap = new BinaryHeap(function (node) {
        return node.f;
      });

      heap.push(this.makeNode(startVertex));

      while (heap.size() > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        var currentNode = heap.pop();
        var i, n;

        if (currentNode.vertex._id === endVertex._id) {
          var vertices = this.vertexList(currentNode);
          if (config.order !== ArangoTraverser.PRE_ORDER) {
            vertices.reverse();
          }

          n = vertices.length;
          for (i = 0; i < n; ++i) {
            config.visitor(config, result, vertices[i].vertex, this.buildPath(vertices[i]));
          }
          return;
        }

        currentNode.closed = true;

        var path = this.buildPath(currentNode);
        var connected = config.expander(config, currentNode.vertex, path);
        n = connected.length;

        for (i = 0; i < n; ++i) {
          var neighbor = this.makeNode(connected[i].vertex);

          if (neighbor.closed) {
            continue;
          }

          var gScore = currentNode.g + 1; // + neighbor.cost
          var beenVisited = neighbor.visited;

          if (!beenVisited || gScore < neighbor.g) {
            var edge = connected[i].edge;
            neighbor.visited = true;
            neighbor.parent = currentNode;
            neighbor.parentEdge = edge;
            neighbor.h = 1;
            if (config.distance && !neighbor.h) {
              neighbor.h = config.distance(config, neighbor.vertex, endVertex, edge);
            }
            neighbor.g = gScore;
            neighbor.f = neighbor.g + neighbor.h;

            if (!beenVisited) {
              heap.push(neighbor);
            } else {
              heap.rescoreElement(neighbor);
            }
          }
        }
      }
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief traversal constructor
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser = function (config) {
  var defaults = {
      order: ArangoTraverser.PRE_ORDER,
      itemOrder: ArangoTraverser.FORWARD,
      strategy: ArangoTraverser.DEPTH_FIRST,
      uniqueness: {
        vertices: ArangoTraverser.UNIQUE_NONE,
        edges: ArangoTraverser.UNIQUE_PATH
      },
      visitor: trackingVisitor,
      filter: null,
      expander: outboundExpander,
      datasource: null,
      maxIterations: 10000000,
      minDepth: 0,
      maxDepth: 256,
      buildVertices: true
    }, d;

  var err;

  if (typeof config !== 'object') {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message;
    throw err;
  }

  // apply defaults
  for (d in defaults) {
    if (defaults.hasOwnProperty(d)) {
      if (!config.hasOwnProperty(d) || config[d] === undefined) {
        config[d] = defaults[d];
      }
    }
  }

  function validate (value, map, param) {
    var m;

    if (value === null || value === undefined) {
      // use first key from map
      for (m in map) {
        if (map.hasOwnProperty(m)) {
          value = m;
          break;
        }
      }
    }
    if (typeof value === 'string') {
      value = value.toLowerCase().replace(/-/, '');
      if (map[value] !== null && map[value] !== undefined) {
        return map[value];
      }
    }
    for (m in map) {
      if (map.hasOwnProperty(m)) {
        if (map[m] === value) {
          return value;
        }
      }
    }

    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'invalid value for ' + param;
    throw err;
  }

  config.uniqueness = {
    vertices: validate(config.uniqueness && config.uniqueness.vertices, {
      none: ArangoTraverser.UNIQUE_NONE,
      global: ArangoTraverser.UNIQUE_GLOBAL,
      path: ArangoTraverser.UNIQUE_PATH
    }, 'uniqueness.vertices'),
    edges: validate(config.uniqueness && config.uniqueness.edges, {
      path: ArangoTraverser.UNIQUE_PATH,
      none: ArangoTraverser.UNIQUE_NONE,
      global: ArangoTraverser.UNIQUE_GLOBAL
    }, 'uniqueness.edges')
  };

  config.strategy = validate(config.strategy, {
    depthfirst: ArangoTraverser.DEPTH_FIRST,
    breadthfirst: ArangoTraverser.BREADTH_FIRST,
    astar: ArangoTraverser.ASTAR_SEARCH,
    dijkstra: ArangoTraverser.DIJKSTRA_SEARCH,
    dijkstramulti: ArangoTraverser.DIJKSTRA_SEARCH_MULTI
  }, 'strategy');

  config.order = validate(config.order, {
    preorder: ArangoTraverser.PRE_ORDER,
    postorder: ArangoTraverser.POST_ORDER,
    preorderexpander: ArangoTraverser.PRE_ORDER_EXPANDER
  }, 'order');

  config.itemOrder = validate(config.itemOrder, {
    forward: ArangoTraverser.FORWARD,
    backward: ArangoTraverser.BACKWARD
  }, 'itemOrder');

  if (typeof config.visitor !== 'function') {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'invalid visitor function';
    throw err;
  }

  // prepare an array of filters
  var filters = [];
  if (config.minDepth !== undefined &&
    config.minDepth !== null &&
    config.minDepth > 0) {
    filters.push(minDepthFilter);
  }
  if (config.maxDepth !== undefined &&
    config.maxDepth !== null &&
    config.maxDepth > 0) {
    filters.push(maxDepthFilter);
  }

  if (!Array.isArray(config.filter)) {
    if (typeof config.filter === 'function') {
      config.filter = [ config.filter ];
    } else {
      config.filter = [];
    }
  }

  config.filter.forEach(function (f) {
    if (typeof f !== 'function') {
      err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
      err.errorMessage = 'invalid filter function';
      throw err;
    }

    filters.push(f);
  });

  if (filters.length > 1) {
    // more than one filter. combine their results
    config.filter = function (config, vertex, path) {
      return combineFilters(filters, config, vertex, path);
    };
  } else if (filters.length === 1) {
    // exactly one filter
    config.filter = filters[0];
  } else {
    config.filter = visitAllFilter;
  }

  if (typeof config.expander !== 'function') {
    config.expander = validate(config.expander, {
      outbound: outboundExpander,
      inbound: inboundExpander,
      any: anyExpander
    }, 'expander');
  }

  if (typeof config.expander !== 'function') {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'invalid expander function';
    throw err;
  }

  if (typeof config.datasource !== 'object') {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'invalid datasource';
    throw err;
  }

  this.config = config;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief execute the traversal
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.prototype.traverse = function (result, startVertex, endVertex) {
  // get the traversal strategy
  var strategy;

  if (this.config.strategy === ArangoTraverser.ASTAR_SEARCH) {
    strategy = astarSearch();
  } else if (this.config.strategy === ArangoTraverser.DIJKSTRA_SEARCH) {
    strategy = dijkstraSearch();
  } else if (this.config.strategy === ArangoTraverser.DIJKSTRA_SEARCH_MULTI) {
    strategy = dijkstraSearchMulti();
  } else if (this.config.strategy === ArangoTraverser.BREADTH_FIRST) {
    strategy = breadthFirstSearch();
  } else {
    strategy = depthFirstSearch();
  }

  // check the start vertex
  if (startVertex === undefined ||
    startVertex === null ||
    typeof startVertex !== 'object') {
    var err1 = new ArangoError();
    err1.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err1.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message +
      ': invalid startVertex specified for traversal';
    throw err1;
  }

  if (strategy.requiresEndVertex() &&
    (endVertex === undefined ||
    endVertex === null ||
    typeof endVertex !== 'object')) {
    var err2 = new ArangoError();
    err2.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err2.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message +
      ': invalid endVertex specified for traversal';
    throw err2;
  }

  // run the traversal
  try {
    strategy.run(this.config, result, startVertex, endVertex);
  } catch (err3) {
    if (typeof err3 !== 'object' || !err3._intentionallyAborted) {
      throw err3;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief every element can be revisited
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_NONE = 0;

// //////////////////////////////////////////////////////////////////////////////
// / @brief element can only be revisited if not already in current path
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_PATH = 1;

// //////////////////////////////////////////////////////////////////////////////
// / @brief element can only be revisited if not already visited
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_GLOBAL = 2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief visitation strategy breadth first
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.BREADTH_FIRST = 0;

// //////////////////////////////////////////////////////////////////////////////
// / @brief visitation strategy depth first
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.DEPTH_FIRST = 1;

// //////////////////////////////////////////////////////////////////////////////
// / @brief astar search
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.ASTAR_SEARCH = 2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief dijkstra search
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.DIJKSTRA_SEARCH = 3;

// //////////////////////////////////////////////////////////////////////////////
// / @brief dijkstra search with multiple end vertices
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.DIJKSTRA_SEARCH_MULTI = 4;

// //////////////////////////////////////////////////////////////////////////////
// / @brief pre-order traversal, visitor called before expander
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRE_ORDER = 0;

// //////////////////////////////////////////////////////////////////////////////
// / @brief post-order traversal
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.POST_ORDER = 1;

// //////////////////////////////////////////////////////////////////////////////
// / @brief pre-order traversal, visitor called at expander
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRE_ORDER_EXPANDER = 2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief forward item processing order
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.FORWARD = 0;

// //////////////////////////////////////////////////////////////////////////////
// / @brief backward item processing order
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.BACKWARD = 1;

// //////////////////////////////////////////////////////////////////////////////
// / @brief prune "constant"
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRUNE = 'prune';

// //////////////////////////////////////////////////////////////////////////////
// / @brief exclude "constant"
// //////////////////////////////////////////////////////////////////////////////

ArangoTraverser.EXCLUDE = 'exclude';

exports.collectionDatasourceFactory = collectionDatasourceFactory;
exports.generalGraphDatasourceFactory = generalGraphDatasourceFactory;

exports.outboundExpander = outboundExpander;
exports.inboundExpander = inboundExpander;
exports.anyExpander = anyExpander;
exports.expandOutEdgesWithLabels = expandOutEdgesWithLabels;
exports.expandInEdgesWithLabels = expandInEdgesWithLabels;
exports.expandEdgesWithLabels = expandEdgesWithLabels;

exports.trackingVisitor = trackingVisitor;
exports.countingVisitor = countingVisitor;
exports.doNothingVisitor = doNothingVisitor;

exports.visitAllFilter = visitAllFilter;
exports.maxDepthFilter = maxDepthFilter;
exports.minDepthFilter = minDepthFilter;
exports.includeMatchingAttributesFilter = includeMatchingAttributesFilter;
exports.abortedException = abortedException;

exports.Traverser = ArangoTraverser;
