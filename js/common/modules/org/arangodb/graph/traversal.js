////////////////////////////////////////////////////////////////////////////////
/// @brief Traversal "classes"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var graph = require("org/arangodb/graph");

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

function ArangoTraverser (config) {
  var defaults = {
    order: ArangoTraverser.PRE_ORDER,
    itemOrder: ArangoTraverser.FORWARD,
    strategy: ArangoTraverser.DEPTH_FIRST,
    uniqueness: {
      vertices: ArangoTraverser.UNIQUE_NONE,
      edges: ArangoTraverser.UNIQUE_NONE
    },
    visitor: TrackingVisitor,
    filter: VisitAllFilter,
    expander: OutboundExpander,
    datasource: null
  };

  if (typeof config !== "object") {
    throw "invalid configuration";
  }

  // apply defaults
  for (var d in defaults) {
    if (! defaults.hasOwnProperty(d)) {
      continue;
    }

    if (! config.hasOwnProperty(d)) {
      config[d] = defaults[d];
    }
  }

  if (typeof config.visitor !== "function") {
    throw "invalid visitor";
  }
  
  if (Array.isArray(config.filter)) {
    config.filter.forEach( function (f) {
      if (typeof f !== "function") {
        throw "invalid filter";
      }
    });
    var innerFilters = config.filter.slice()
    var combinedFilter = function(config, vertex, path) {
      return CombineFilters(innerFilters, config, vertex, path);
    };
    config.filter = combinedFilter;
  }
  
  if (typeof config.filter !== "function") {
    throw "invalid filter";
  }

  if (typeof config.expander !== "function") {
    throw "invalid expander";
  }

  if (typeof config.datasource !== "object") {
    throw "invalid datasource";
  }

  this.config = config;
}

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
  if (startVertex == undefined || startVertex == null) {
    throw "invalid startVertex specified for traversal";
  }

  // get the traversal strategy
  var strategy;
  if (this.config.strategy === ArangoTraverser.BREADTH_FIRST) {
    strategy = BreadthFirstSearch();
  }
  else {
    strategy = DepthFirstSearch();
  }
 
  // run the traversal
  strategy.run(this.config, result, startVertex);
};

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
/// @brief parse a filter result
////////////////////////////////////////////////////////////////////////////////

function ParseFilterResult (args) {
  var result = {
    visit: true,
    expand: true
  };

  function processArgument (arg) {
    if (arg == undefined || arg == null) {
      return;
    } 

    if (typeof(arg) === 'string') {
      if (arg === ArangoTraverser.EXCLUDE) {
        result.visit = false;
        return;
      }
      else if (arg === ArangoTraverser.PRUNE) {
        result.expand = false;
        return;
      }
      else if (arg === '') {
        return;
      }
    }
    else if (Array.isArray(arg)) {
      for (var i = 0; i < arg.length; ++i) {
        processArgument(arg[i]);
      }
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

function CheckUniqueness (uniqueness, visited, vertex, edge) {
  if (uniqueness.vertices !== ArangoTraverser.UNIQUE_NONE) {
    if (visited.vertices[vertex._id] === true) {
      return false;
    }

    visited.vertices[vertex._id] = true;
  }

  if (edge != null && uniqueness.edges !== ArangoTraverser.UNIQUE_NONE) {
    if (visited.edges[edge._id] === true) {
      return false;
    }

    visited.edges[edge._id] = true;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if we must process items in reverse order
////////////////////////////////////////////////////////////////////////////////

function CheckReverse (config) {
  if (config.order === ArangoTraverser.POST_ORDER) {
    // post order
    if (config.itemOrder === ArangoTraverser.FORWARD) {
      return true;
    }
  }
  else if (config.order  === ArangoTraverser.PRE_ORDER) {
    // pre order
    if (config.itemOrder === ArangoTraverser.BACKWARD && 
        config.strategy  === ArangoTraverser.BREADTH_FIRST) {
      return true;
    }
    if (config.itemOrder === ArangoTraverser.FORWARD && 
        config.strategy  === ArangoTraverser.DEPTH_FIRST) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for breadth-first strategy
////////////////////////////////////////////////////////////////////////////////

function BreadthFirstSearch () {
  return {
    getPathItems: function (items) {
      var visited = { };
      var ignore = items.length - 1;
      
      items.forEach(function (item, i) {
        if (i != ignore) {
          visited[item._id] = true;
        }
      });

      return visited;
    },

    createPath: function (items, idx) {
      var path = { edges: [ ], vertices: [ ] };
      var pathItem = items[idx];
      
      while (true) {
        if (pathItem.edge != null) {
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
      var toVisit = [ { edge: null, vertex: startVertex, parentIndex: -1 } ];
      var visited = { edges: { }, vertices: { } };

      var index = 0;
      var step = 1;
      var reverse = CheckReverse(config);

      while ((step == 1 && index < toVisit.length) ||
             (step == -1 && index >= 0)) {
        var current = toVisit[index];
        var vertex  = current.vertex;
        var edge    = current.edge;

        if (current.visit == null) {
          current.visit = false;
          
          var path = this.createPath(toVisit, index);
          
          // first apply uniqueness check
          if (config.uniqueness.vertices === ArangoTraverser.UNIQUE_PATH) {
            visited.vertices = this.getPathItems(path.vertices);
          }
          if (config.uniqueness.edges === ArangoTraverser.UNIQUE_PATH) {
            visited.edges = this.getPathItems(path.edges);
          }

          if (! CheckUniqueness(config.uniqueness, visited, vertex, edge)) {
            if (index < toVisit.length - 1) {
              index += step;
            }
            else {
              step = -1;
            }
            continue;
          }

          var filterResult = ParseFilterResult(config.filter(config, vertex, path));
          if (config.order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
            // preorder
            config.visitor(config, result, vertex, path);
          }
          else {
            // postorder
            current.visit = filterResult.visit || false;
          }

          if (filterResult.expand) {
            var connected = config.expander(config, vertex, path);
            if (connected.length > 0) {
              reverse && connected.reverse();
              for (var i = 0; i < connected.length; ++i) {
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
            var path = this.createPath(toVisit, index);
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

function DepthFirstSearch () {
  return {
    getPathItems: function (items) {
      var visited = { };
      items.forEach(function (item) {
        visited[item._id] = true;
      });
      return visited;
    },

    run: function (config, result, startVertex) {
      var toVisit = [ { edge: null, vertex: startVertex, visit: null } ];
      var path    = { edges: [ ], vertices: [ ] };
      var visited = { edges: { }, vertices: { } };
      var reverse = CheckReverse(config);

      while (toVisit.length > 0) {
        // peek at the top of the stack
        var current = toVisit[toVisit.length - 1];
        var vertex  = current.vertex;
        var edge    = current.edge;

        // check if we visit the element for the first time
        if (current.visit == null) {
          current.visit = false;

          // first apply uniqueness check
          if (config.uniqueness.vertices === ArangoTraverser.UNIQUE_PATH) {
            visited.vertices = this.getPathItems(path.vertices);
          }
          if (config.uniqueness.edges === ArangoTraverser.UNIQUE_PATH) {
            visited.edges = this.getPathItems(path.edges);
          }

          if (! CheckUniqueness(config.uniqueness, visited, vertex, edge)) {
            // skip element if not unique
            toVisit.pop();
            continue;
          }

          // push the current element onto the path stack
          if (edge != null) {
            path.edges.push(edge);
          }
          path.vertices.push(vertex);

          var filterResult = ParseFilterResult(config.filter(config, vertex, path));
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
            var connected = config.expander(config, vertex, path);
            if (connected.length > 0) {
              reverse && connected.reverse();
              for (var i = 0; i < connected.length; ++i) {
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
/// @brief default outbound expander function
////////////////////////////////////////////////////////////////////////////////

function OutboundExpander (config, vertex, path) {
  var connections = [ ];
  var outEdges = config.datasource.getOutEdges(vertex._id);
      
  if (outEdges.length > 1 && config.sort) {
    outEdges.sort(config.sort);
  }

  outEdges.forEach(function (edge) {
    try {
      var v = config.datasource.getVertex(edge._to);
      connections.push({ edge: edge, vertex: v });    
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

function InboundExpander (config, vertex, path) {
  var connections = [ ];
  var inEdges = config.datasource.getInEdges(vertex._id);
      
  if (inEdges.length > 1 && config.sort) {
    inEdges.sort(config.sort);
  }

  inEdges.forEach(function (edge) {
    try {
      var v = config.datasource.getVertex(edge._from);
      connections.push({ edge: edge, vertex: v });    
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

function AnyExpander (config, vertex, path) {
  var connections = [ ];
  var edges = config.datasource.getAllEdges(vertex._id);
      
  if (edges.length > 1 && config.sort) {
    edges.sort(config.sort);
  }

  edges.forEach(function (edge) {
    try {
      var v = config.datasource.getVertex(edge._from === vertex._id ? edge._to : edge._from);
      connections.push({ edge: edge, vertex: v });    
    }
    catch (e) {
      // continue even in the face of non-existing documents
    }
  });
      
  return connections;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default ArangoCollection datasource
///
/// this is a factory function that creates a datasource that operates on the
/// specified edge collection
////////////////////////////////////////////////////////////////////////////////

function CollectionDatasourceFactory (edgeCollection) {
  return {
    edgeCollection: edgeCollection,

    getAllEdges: function (vertexId) {
      return this.edgeCollection.edges(vertexId);
    },
    
    getInEdges: function (vertexId) {
      return this.edgeCollection.inEdges(vertexId);
    },
    
    getOutEdges: function (vertexId) {
      return this.edgeCollection.outEdges(vertexId);
    },

    getVertex: function (vertexId) {
      return internal.db._document(vertexId);
    }
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief default Graph datasource
///
/// this is a datasource that operates on the specified graph
////////////////////////////////////////////////////////////////////////////////

function GraphDatasourceFactory (name) {
  return {
    graph: new graph.Graph(name),

    getAllEdges: function (vertexId) {
      var r = [ ];
      var that = this;
      this.graph.getVertex(vertexId).getEdges().forEach(function (edge) {
        var vertexIn = edge.getInVertex();
        var vertexOut = edge.getOutVertex();
        r.push({ _id: edge._id, _to: vertexIn._id.split("/")[1], _from: vertexOut._id.split("/")[1] });
      });

      return r;
    },
    
    getInEdges: function (vertexId) {
      var r = [ ];
      var that = this;
      this.graph.getVertex(vertexId).getInEdges().forEach(function (edge) {
        var vertexIn = edge.getInVertex();
        var vertexOut = edge.getOutVertex();
        r.push({ _id: edge._id, _to: vertexIn._id.split("/")[1], _from: vertexOut._id.split("/")[1] });
      });

      return r;
    },
    
    getOutEdges: function (vertexId) {
      var r = [ ];
      var that = this;
      this.graph.getVertex(vertexId).getOutEdges().forEach(function (edge) {
        var vertexIn = edge.getInVertex();
        var vertexOut = edge.getOutVertex();
        r.push({ _id: edge._id, _to: vertexIn._id.split("/")[1], _from: vertexOut._id.split("/")[1] });
      });

      return r;
    },

    getVertex: function (vertexId) {
      return this.graph.getVertex(vertexId);
    }
  };
};

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief default expander that expands all outbound edges labeled with one label in config.labels 
///////////////////////////////////////////////////////////////////////////////////////////

function ExpandOutEdgesWithLabels (config, vertex, path) {
  var result = [ ];
  if (! Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }
  var edgesList = config.datasource.getOutEdges(vertex._id);
  if (edgesList != undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      if (!!~config.labels.indexOf(edgesList[i].label)) {
        result.push({ edge: edgesList[i], vertex: config.datasource.vertices[edgesList[i]._to] });
      }
    }
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief default expander that expands all inbound edges labeled with one label in config.labels 
///////////////////////////////////////////////////////////////////////////////////////////
  
function ExpandInEdgesWithLabels (config, vertex, path) {
  var result = [ ];
  if (!Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }
  var edgesList = config.datasource.getInEdges(vertex._id);
  if (edgesList != undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      if (!!~config.labels.indexOf(edgesList[i].label)) {
        result.push({ edge: edgesList[i], vertex: config.datasource.vertices[edgesList[i]._from] });
      }
    }
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief default expander that expands all edges labeled with one label in config.labels 
///////////////////////////////////////////////////////////////////////////////////////////  
  
function ExpandEdgesWithLabels (config, vertex, path) {
  var result = [ ];
  if (! Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }
  var edgesList = config.datasource.getInEdges(vertex._id);
  if (edgesList != undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      if (!!~config.labels.indexOf(edgesList[i].label)) {
        result.push({ edge: edgesList[i], vertex: config.datasource.vertices[edgesList[i]._from] });
      }
    }
  }
  edgesList = config.datasource.getOutEdges(vertex._id);
  if (edgesList != undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      if (!!~config.labels.indexOf(edgesList[i].label)) {
        result.push({ edge: edgesList[i], vertex: config.datasource.vertices[edgesList[i]._to] });
      }
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default visitor that just tracks every visit
////////////////////////////////////////////////////////////////////////////////

function TrackingVisitor (config, result, vertex, path) {
  if (! result || ! result.visited) {
    return;
  }

  function clone (obj) {
    if (obj == null || typeof(obj) !== "object") {
      return obj;
    }

    if (Array.isArray(obj)) {
      var copy = [];
      for (var i = 0; i < obj.length; ++i) {
        copy[i] = clone(obj[i]);
      }
    }
    else if (obj instanceof Object) {
      var copy = {};
      for (var attr in obj) {
        if (obj.hasOwnProperty && obj.hasOwnProperty(attr)) {
          copy[attr] = clone(obj[attr]);
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
/// @brief default filter to visit & expand all vertices
////////////////////////////////////////////////////////////////////////////////

function VisitAllFilter () {
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default filter to visit & expand all vertices up to a given depth
////////////////////////////////////////////////////////////////////////////////

function MaxDepthFilter (config, vertex, path) {
  if (path.vertices.length > config.maxDepth) {
    return ArangoTraverser.PRUNE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default filter to exclude all vertices up to a given depth
////////////////////////////////////////////////////////////////////////////////

function MinDepthFilter (config, vertex, path) {
  if (path.vertices.length <= config.minDepth) {
    return ArangoTraverser.EXCLUDE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default filter to include all vertices matching one of the given 
/// attribute sets
////////////////////////////////////////////////////////////////////////////////

function IncludeMatchingAttributesFilter (config, vertex, path) {
  if (! Array.isArray(config.matchingAttributes)) {
    config.matchingAttributes = [config.matchingAttributes];
  }
  var include = false
  config.matchingAttributes.forEach( function(example) {
    var count = 0;
    Object.keys(example).forEach( function (key) {
      if (vertex[key] && vertex[key] === example[key]) {
        count++;
      }
    });
    if (count > 0 && count === Object.keys(example).length) {
      include = true;
      return;
    }
  });
  if (!include) {
    return "exclude";
  } else {
    return;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief combine an array of filters
////////////////////////////////////////////////////////////////////////////////

function CombineFilters (filters, config, vertex, path) {
  var result = [];
  filters.forEach( function (f) {
    var tmp = f(config, vertex, path);
    if (!Array.isArray(tmp)) {
      tmp = [tmp];
    }
    result = result.concat(tmp);
  });
  return result;
}

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

exports.Traverser                       = ArangoTraverser;
exports.OutboundExpander                = OutboundExpander;
exports.InboundExpander                 = InboundExpander;
exports.AnyExpander                     = AnyExpander;
exports.CollectionDatasourceFactory     = CollectionDatasourceFactory;
exports.GraphDatasourceFactory          = GraphDatasourceFactory;
exports.VisitAllFilter                  = VisitAllFilter;
exports.IncludeMatchingAttributesFilter = IncludeMatchingAttributesFilter;
exports.TrackingVisitor                 = TrackingVisitor;
exports.MinDepthFilter                  = MinDepthFilter;
exports.MaxDepthFilter                  = MaxDepthFilter;
exports.ExpandEdgesWithLabels           = ExpandEdgesWithLabels;
exports.ExpandInEdgesWithLabels         = ExpandInEdgesWithLabels;
exports.ExpandOutEdgesWithLabels        = ExpandOutEdgesWithLabels;

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
