/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, continue: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Traversal "classes"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2013 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var graph = require("org/arangodb/graph");
var arangodb = require("org/arangodb");
var ArangoError = arangodb.ArangoError;

var db = arangodb.db;

var ArangoTraverser;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                       datasources
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default ArangoCollection datasource
///
/// This is a factory function that creates a datasource that operates on the
/// specified edge collection. The vertices and edges are the documents in the
/// corresponding collections.
////////////////////////////////////////////////////////////////////////////////

function collectionDatasourceFactory (edgeCollection) {
  return {
    edgeCollection: edgeCollection,

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

    getLabel: function (edge) {
      return edge.$label;
    },

    getAllEdges: function (vertex) {
      return this.edgeCollection.edges(vertex._id);
    },
    
    getInEdges: function (vertex) {
      return this.edgeCollection.inEdges(vertex._id);
    },
    
    getOutEdges: function (vertex) {
      return this.edgeCollection.outEdges(vertex._id);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default Graph datasource
///
/// This is a datasource that operates on the specified graph. The vertices
/// are from type Vertex, the edges from type Edge.
////////////////////////////////////////////////////////////////////////////////

function graphDatasourceFactory (name) {
  return {
    graph: new graph.Graph(name),

    getVertexId: function (vertex) {
      return vertex.getId();
    },

    getPeerVertex: function (edge, vertex) {
      return edge.getPeerVertex(vertex);
    },

    getInVertex: function (edge) {
      return edge.getInVertex();
    },

    getOutVertex: function (edge) {
      return edge.getOutVertex();
    },

    getEdgeId: function (edge) {
      return edge.getId();
    },

    getLabel: function (edge) {
      return edge.getLabel();
    },

    getAllEdges: function (vertex) {
      return vertex.edges();
    },
    
    getInEdges: function (vertex) {
      return vertex.inbound();
    },
    
    getOutEdges: function (vertex) {
      return vertex.outbound();
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         expanders
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default outbound expander function
////////////////////////////////////////////////////////////////////////////////

function outboundExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [ ];
  var outEdges = datasource.getOutEdges(vertex);
      
  if (outEdges.length > 1 && config.sort) {
    outEdges.sort(config.sort);
  }

  outEdges.forEach(function (edge) {
    try {
      var v = datasource.getInVertex(edge);

      if (! config.expandFilter || config.expandFilter(config, v, edge, path)) {
        connections.push({ edge: edge, vertex: v });    
      }
    }
    catch (e) {
      // continue even in the face of non-existing documents
    }
  });

  return connections;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default inbound expander function 
////////////////////////////////////////////////////////////////////////////////

function inboundExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [ ];
  var inEdges = datasource.getInEdges(vertex);
      
  if (inEdges.length > 1 && config.sort) {
    inEdges.sort(config.sort);
  }

  inEdges.forEach(function (edge) {
    try {
      var v = datasource.getOutVertex(edge);

      if (! config.expandFilter || config.expandFilter(config, v, edge, path)) {
        connections.push({ edge: edge, vertex: v });    
      }
    }
    catch (e) {
      // continue even in the face of non-existing documents
    }
  });
      
  return connections;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default "any" expander function 
////////////////////////////////////////////////////////////////////////////////

function anyExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [ ];
  var edges = datasource.getAllEdges(vertex);
      
  if (edges.length > 1 && config.sort) {
    edges.sort(config.sort);
  }

  edges.forEach(function (edge) {
    try {
      var v = datasource.getPeerVertex(edge, vertex);

      if (! config.expandFilter || config.expandFilter(config, v, edge, path)) {
        connections.push({ edge: edge, vertex: v });    
      }
    }
    catch (e) {
      // continue even in the face of non-existing documents
    }
  });
      
  return connections;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief expands all outbound edges labeled with at least one label in config.labels 
///////////////////////////////////////////////////////////////////////////////////////////

function expandOutEdgesWithLabels (config, vertex, path) {
  var datasource = config.datasource;
  var result = [ ];
  var i;

  if (! Array.isArray(config.labels)) {
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

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief expands all inbound edges labeled with at least one label in config.labels 
///////////////////////////////////////////////////////////////////////////////////////////
  
function expandInEdgesWithLabels (config, vertex, path) {
  var datasource = config.datasource;
  var result = [ ];
  var i;

  if (! Array.isArray(config.labels)) {
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

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief expands all edges labeled with at least one label in config.labels 
///////////////////////////////////////////////////////////////////////////////////////////  
  
function expandEdgesWithLabels (config, vertex, path) {
  var datasource = config.datasource;
  var result = [ ];
  var i;

  if (! Array.isArray(config.labels)) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          visitors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default visitor that just tracks every visit
////////////////////////////////////////////////////////////////////////////////

function trackingVisitor (config, result, vertex, path) {
  if (! result || ! result.visited) {
    return;
  }

  function clone (obj) {
    if (obj === null || typeof(obj) !== "object") {
      return obj;
    }

    var copy, i;

    if (Array.isArray(obj)) {
      copy = [ ];

      for (i = 0; i < obj.length; ++i) {
        copy[i] = clone(obj[i]);
      }
    }
    else if (obj instanceof Object) {
      copy = { };

      if (obj.hasOwnProperty) {
        for (i in obj) {
          if (obj.hasOwnProperty(i)) {
            copy[i] = clone(obj[i]);
          }
        }
      }
    }

    return copy;
  }

  if (result.visited.vertices) {
    result.visited.vertices.push(clone(vertex));
  }

  if (result.visited.paths) {
    result.visited.paths.push(clone(path));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           filters
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default filter to visit & expand all vertices
////////////////////////////////////////////////////////////////////////////////

function visitAllFilter () {
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter to visit & expand all vertices up to a given depth
////////////////////////////////////////////////////////////////////////////////

function maxDepthFilter (config, vertex, path) {
  if (path.vertices.length > config.maxDepth) {
    return ArangoTraverser.PRUNE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exclude all vertices up to a given depth
////////////////////////////////////////////////////////////////////////////////

function minDepthFilter (config, vertex, path) {
  if (path.vertices.length <= config.minDepth) {
    return ArangoTraverser.EXCLUDE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief include all vertices matching one of the given attribute sets
////////////////////////////////////////////////////////////////////////////////

function includeMatchingAttributesFilter (config, vertex, path) {
  if (! Array.isArray(config.matchingAttributes)) {
    config.matchingAttributes = [config.matchingAttributes];
  }

  var include = false;

  config.matchingAttributes.forEach(function(example) {
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

  if (! include) {
    result = "exclude";
  } 

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief combine an array of filters
////////////////////////////////////////////////////////////////////////////////

function combineFilters (filters, config, vertex, path) {
  var result = [ ];

  filters.forEach(function (f) {
    var tmp = f(config, vertex, path);

    if (! Array.isArray(tmp)) {
      tmp = [ tmp ];
    }

    result = result.concat(tmp);
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a filter result
////////////////////////////////////////////////////////////////////////////////

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

    if (typeof(arg) === 'string') {
      if (arg === ArangoTraverser.EXCLUDE) {
        result.visit = false;
        finish = true;
      }
      else if (arg === ArangoTraverser.PRUNE) {
        result.expand = false;
        finish = true;
      }
      else if (arg === '') {
        finish = true;
      }
    }
    else if (Array.isArray(arg)) {
      var i;
      for (i = 0; i < arg.length; ++i) {
        processArgument(arg[i]);
      }
      finish = true;
    }

    if (finish) {
      return;
    }

    throw "invalid filter result";
  }

  processArgument(args);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the uniqueness checks
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief check if we must process items in reverse order
////////////////////////////////////////////////////////////////////////////////

function checkReverse (config) {
  var result = false;

  if (config.order === ArangoTraverser.POST_ORDER) {
    // post order
    if (config.itemOrder === ArangoTraverser.FORWARD) {
      result = true;
    }
  }
  else if (config.order  === ArangoTraverser.PRE_ORDER) {
    // pre order
    if (config.itemOrder === ArangoTraverser.BACKWARD && 
        config.strategy  === ArangoTraverser.BREADTH_FIRST) {
      result = true;
    }
    else if (config.itemOrder === ArangoTraverser.FORWARD && 
             config.strategy  === ArangoTraverser.DEPTH_FIRST) {
      result = true; 
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for breadth-first strategy
////////////////////////////////////////////////////////////////////////////////

function breadthFirstSearch () {
  return {
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
      var path = { edges: [ ], vertices: [ ] };
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
        var vertex  = current.vertex;
        var edge    = current.edge;
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

          if (! checkUniqueness(config, visited, vertex, edge)) {
            if (index < toVisit.length - 1) {
              index += step;
            }
            else {
              step = -1;
            }
            continue;
          }

          var filterResult = parseFilterResult(config.filter(config, vertex, path));
          if (config.order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
            // preorder
            config.visitor(config, result, vertex, path);
          }
          else {
            // postorder
            current.visit = filterResult.visit || false;
          }

          if (filterResult.expand) {
            var connected = config.expander(config, vertex, path), i;
            if (connected.length > 0) {
              if (reverse) {
                connected.reverse();
              }
              for (i = 0; i < connected.length; ++i) {
                connected[i].parentIndex = index;
                toVisit.push(connected[i]);
              }
            }
          }
            
          if (config.order === ArangoTraverser.POST_ORDER) {
            if (index < toVisit.length - 1) {
              index += step;
            }
            else {
              step = -1;
            }
          }
        }
        else {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for depth-first strategy
////////////////////////////////////////////////////////////////////////////////

function depthFirstSearch () {
  return {
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
      var path    = { edges: [ ], vertices: [ ] };
      var visited = { edges: { }, vertices: { } };
      var reverse = checkReverse(config);

      while (toVisit.length > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        // peek at the top of the stack
        var current = toVisit[toVisit.length - 1];
        var vertex  = current.vertex;
        var edge    = current.edge;

        // check if we visit the element for the first time
        if (current.visit === null || current.visit === undefined) {
          current.visit = false;

          // first apply uniqueness check
          if (config.uniqueness.vertices === ArangoTraverser.UNIQUE_PATH) {
            visited.vertices = this.getPathItems(config.datasource.getVertexId, path.vertices);
          }

          if (config.uniqueness.edges === ArangoTraverser.UNIQUE_PATH) {
            visited.edges = this.getPathItems(config.datasource.getEdgeId, path.edges);
          }

          if (! checkUniqueness(config, visited, vertex, edge)) {
            // skip element if not unique
            toVisit.pop();
            continue;
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
          }
          else {
            // postorder. mark the element visitation flag because we'll got to check it later
            current.visit = filterResult.visit || false;
          }

          // expand the element's children?
          if (filterResult.expand) {
            var connected = config.expander(config, vertex, path), i;
            if (connected.length > 0) {
              if (reverse) {
                connected.reverse();
              }
              for (i = 0; i < connected.length; ++i) {
                connected[i].visit = null;
                toVisit.push(connected[i]);
              }
            }
          }
        }
        else {
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


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoTraverser
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief traversal constructor
////////////////////////////////////////////////////////////////////////////////

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
    filter: visitAllFilter,
    expander: outboundExpander,
    datasource: null,
    maxIterations: 10000,
    minDepth: 0,
    maxDepth: 256
  }, d;

  var err;

  if (typeof config !== "object") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message;
    throw err;
  }

  // apply defaults
  for (d in defaults) {
    if (defaults.hasOwnProperty(d)) {
      if (! config.hasOwnProperty(d) || config[d] === undefined) {
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
      value = value.toLowerCase().replace(/-/, "");
      if (map[value] !== null) {
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
    err.errorMessage = "invalid value for " + param;
    throw err;
  }
    
  config.uniqueness = {
    vertices: validate(config.uniqueness && config.uniqueness.vertices, {
      none:   ArangoTraverser.UNIQUE_NONE,
      global: ArangoTraverser.UNIQUE_GLOBAL,
      path:   ArangoTraverser.UNIQUE_PATH
    }, "uniqueness.vertices"),
    edges: validate(config.uniqueness && config.uniqueness.edges, {
      path:   ArangoTraverser.UNIQUE_PATH,
      none:   ArangoTraverser.UNIQUE_NONE,
      global: ArangoTraverser.UNIQUE_GLOBAL
    }, "uniqueness.edges")
  };
  
  config.strategy = validate(config.strategy, {
    depthfirst: ArangoTraverser.DEPTH_FIRST,
    breadthfirst: ArangoTraverser.BREADTH_FIRST
  }, "strategy");

  config.order = validate(config.order, {
    preorder: ArangoTraverser.PRE_ORDER,
    postorder: ArangoTraverser.POST_ORDER
  }, "order");

  config.itemOrder = validate(config.itemOrder, {
    forward: ArangoTraverser.FORWARD,
    backward: ArangoTraverser.BACKWARD
  }, "itemOrder");


  if (typeof config.visitor !== "function") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid visitor function";
    throw err;
  }
  
  // prepare an array of filters
  var filters = [ ];
  if (config.minDepth !== undefined && config.minDepth >= 0) {
    filters.push(minDepthFilter);
  }
  if (config.maxDepth !== undefined && config.maxDepth > 0) {
    filters.push(maxDepthFilter);
  }

  if (! Array.isArray(config.filter)) {
    config.filter = [ config.filter ];
  }

  config.filter.forEach( function (f) {
    if (typeof f !== "function") {
      err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
      err.errorMessage = "invalid filter function";
      throw err;
    }

    filters.push(f);
  });

  if (filters.length === 0) {
    filters.push(visitAllFilter);
  }

  config.filter = function (config, vertex, path) {
    return combineFilters(filters, config, vertex, path);
  };

  if (typeof config.expander !== "function") {
    config.expander = validate(config.expander, {
      outbound: outboundExpander,
      inbound: inboundExpander,
      any: anyExpander
    }, "expander");
  }

  if (typeof config.expander !== "function") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid expander function";
    throw err;
  }

  if (typeof config.datasource !== "object") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid datasource";
    throw err;
  }

  this.config = config;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the traversal
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.prototype.traverse = function (result, startVertex) { 
  // check the start vertex
  if (startVertex === undefined || startVertex === null) {
    throw "invalid startVertex specified for traversal";
  }

  // get the traversal strategy
  var strategy;
  if (this.config.strategy === ArangoTraverser.BREADTH_FIRST) {
    strategy = breadthFirstSearch();
  }
  else {
    strategy = depthFirstSearch();
  }
 
  // run the traversal
  strategy.run(this.config, result, startVertex);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief every element can be revisited
////////////////////////////////////////////////////////////////////////////////
  
ArangoTraverser.UNIQUE_NONE          = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief element can only be revisited if not already in current path
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_PATH          = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief element can only be revisited if not already visited
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_GLOBAL        = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief visitation strategy breadth first
////////////////////////////////////////////////////////////////////////////////
  
ArangoTraverser.BREADTH_FIRST        = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief visitation strategy depth first
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.DEPTH_FIRST          = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief pre-order traversal
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRE_ORDER            = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief post-order traversal
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.POST_ORDER           = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief forward item processing order
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.FORWARD              = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief backward item processing order
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.BACKWARD             = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief prune "constant"
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRUNE                = 'prune';

////////////////////////////////////////////////////////////////////////////////
/// @brief exclude "constant"
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.EXCLUDE              = 'exclude';

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.collectionDatasourceFactory     = collectionDatasourceFactory;
exports.graphDatasourceFactory          = graphDatasourceFactory;

exports.outboundExpander                = outboundExpander;
exports.inboundExpander                 = inboundExpander;
exports.anyExpander                     = anyExpander;
exports.expandOutEdgesWithLabels        = expandOutEdgesWithLabels;
exports.expandInEdgesWithLabels         = expandInEdgesWithLabels;
exports.expandEdgesWithLabels           = expandEdgesWithLabels;

exports.trackingVisitor                 = trackingVisitor;

exports.visitAllFilter                  = visitAllFilter;
exports.maxDepthFilter                  = maxDepthFilter;
exports.minDepthFilter                  = minDepthFilter;
exports.includeMatchingAttributesFilter = includeMatchingAttributesFilter;

exports.Traverser                       = ArangoTraverser;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
