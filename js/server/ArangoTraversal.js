////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoTraverser
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

(function () {
  var internal = require("internal"),
      console  = require("console");

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

  function ArangoTraverser (edgeCollection,
                            visitationStrategy,
                            order,
                            uniqueness, 
                            visitor, 
                            filter, 
                            expander) {
    // check edgeCollection
    if (typeof edgeCollection === "string") {
      edgeCollection = internal.db._collection(edgeCollection);
    }
    
    if (! (edgeCollection instanceof ArangoCollection) ||
        edgeCollection.type() != internal.ArangoCollection.TYPE_EDGE) {
      throw "invalid edgeCollection";
    }
    
    this._edgeCollection       = edgeCollection;

    // check the visitation strategy    
    if (visitationStrategy !== ArangoTraverser.BREADTH_FIRST &&
        visitationStrategy !== ArangoTraverser.DEPTH_FIRST) {
      throw "invalid visitationStrategy";
    }

    if (visitationStrategy === ArangoTraverser.BREADTH_FIRST) {
      this._visitationStrategy = BreadthFirstSearch();
    }
    else {
      this._visitationStrategy = DepthFirstSearch();
    }

    // check the visitation order
    if (order !== ArangoTraverser.PRE_ORDER &&
        order !== ArangoTraverser.POST_ORDER) {
      throw "invalid order";
    }
    this._order                = order;
    
    // initialise uniqueness check attributes
    this._uniqueness           = { 
      vertices: uniqueness.vertices || ArangoTraverser.UNIQUE_NONE,
      edges:    uniqueness.edges    || ArangoTraverser.UNIQUE_NONE
    };

    // callbacks
    this._visitor              = visitor;
    this._filter               = filter || IncludeAllFilter;
    this._expander             = expander || OutboundExpander;
    
    if (typeof this._visitor !== "function") {
      throw "invalid visitor";
    }
    
    if (typeof this._filter !== "function") {
      throw "invalid filter";
    }
    
    if (typeof this._expander !== "function") {
      throw "invalid expander";
    }
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

  ArangoTraverser.prototype.traverse = function (startVertex, context) { 
    // check the start vertex
    if (startVertex == undefined) {
      throw "invalid startVertex specified for traversal";
    }
    if (typeof startVertex == "string") {
      startVertex = internal.db._document(startVertex);
    }

    // set user defined context
    this._context = context;
   
    // run the traversal
    this._visitationStrategy.run(this, startVertex);
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
/// @brief implementation details for breadth-first strategy
////////////////////////////////////////////////////////////////////////////////

  function BreadthFirstSearch () {
    return {
      run: function (traverser, startVertex) {
        var toVisit = [ { edge: null, vertex: startVertex, parentIndex: -1 } ];
        var visited = { edges: { }, vertices: { } };

        var index = 0;

        while (index >= 0 && index < toVisit.length) {
          var current = toVisit[index];
          var vertex  = current.vertex;
          var edge    = current.edge;

          if (current.visit == null) {
            current.visit = false;
            
            // first apply uniqueness check
            if (! CheckUniqueness(traverser._uniqueness, visited, vertex, edge)) {
              index++;
              continue;
            }

            var path = { edges: [ ], vertices: [ ] };
            var pathItem = current;
            while (true) {
              if (pathItem.edge != null) {
                path.edges.unshift(pathItem.edge);
              }
              path.vertices.unshift(pathItem.vertex);
              var idx = pathItem.parentIndex;
              if (idx < 0) {
                break;
              }
              pathItem = toVisit[idx];
            }

            var filterResult = traverser._filter(traverser, vertex, path);
            if (traverser._order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
              // preorder
              traverser._visitor(traverser, vertex, path);
            }
            else {
              // postorder
              current.visit = filterResult.visit || false;
            }

            if (filterResult.expand) {
              var connected = traverser._expander(traverser, vertex, path);
              if (connected.length > 0) {
                for (var i = 0; i < connected.length; ++i) {
                  connected[i].parentIndex = index;
                  toVisit.push(connected[i]);
                }
              }
              if (traverser._order === ArangoTraverser.POST_ORDER) {
                index = toVisit.length - 1;
              }
            }
          }
          else {
            if (traverser._order === ArangoTraverser.POST_ORDER) {
              index--;
            }
            else {
              index++;
            }
            
            if (traverser._order === ArangoTraverser.POST_ORDER && current.visit) {
              traverser._visitor(traverser, vertex, path);
            }
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

      run: function (traverser, startVertex) {
        var toVisit = [ { edge: null, vertex: startVertex, visit: null } ];
        var path    = { edges: [ ], vertices: [ ] };
        var visited = { edges: { }, vertices: { } };

        while (toVisit.length > 0) {
          var current = toVisit[toVisit.length - 1];
          var vertex  = current.vertex;
          var edge    = current.edge;

          if (current.visit == null) {
            current.visit = false;

            // first apply uniqueness check
            if (! CheckUniqueness(traverser._uniqueness, visited, vertex, edge)) {
              toVisit.pop();
              continue;
            }

            if (edge) {
              path.edges.push(edge);
            }
            path.vertices.push(vertex);

            var filterResult = traverser._filter(traverser, vertex, path);
            if (traverser._order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
              // preorder
              traverser._visitor(traverser, vertex, path);
            }
            else {
              // postorder
              current.visit = filterResult.visit || false;
            }

            if (filterResult.expand) {
              var connected = traverser._expander(traverser, vertex, path);
              if (connected.length > 0) {
                for (var i = connected.length - 1; i >= 0; --i) {
                  connected[i].visit = null;
                  toVisit.push(connected[i]);
                }
              }
            }
          }
          else {
            toVisit.pop();
            if (traverser._order === ArangoTraverser.POST_ORDER && current.visit) {
              traverser._visitor(traverser, vertex, path);
            }
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
/// @brief default filter function (if none specified)
///
/// this filter will simply include every vertex
////////////////////////////////////////////////////////////////////////////////

  function IncludeAllFilter (traverser, vertex, path) {
    return {
      visit:  true,
      expand: true
    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief default outbound expander function
////////////////////////////////////////////////////////////////////////////////

  function OutboundExpander (traverser, vertex, path) {
    var connections = [ ];

    this._edgeCollection.outEdges(vertex._id).forEach(function (edge) {
      var vertex = internal.db._document(edge._to);
      connections.push({ edge: edge, vertex: vertex });    
    });

    return connections;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief default inbound expander function 
////////////////////////////////////////////////////////////////////////////////

  function InboundExpander (traverser, vertex, path) {
    var connections = [ ];

    this._edgeCollection.inEdges(vertex._id).forEach(function (edge) {
      var vertex = internal.db._document(edge._from);
      connections.push({ edge: edge, vertex: vertex });    
    });

    return connections;
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
/// @brief element may be revisited
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
/// @}
////////////////////////////////////////////////////////////////////////////////

  var stringifyPath = function (path) {
    var result = "[";
    for (var i = 0; i < path.edges.length; ++i) {
      if (i > 0) {
        result += ", ";
      }
      result += path.edges[i].what;
    }

    result += " ]";

    return result;

  }


  var context = { someContext : true };

  var visitor = function (traverser, vertex, path) {
    var result = path.edges.length + " ";
    for (var i = 0; i < path.edges.length; ++i) {
      result += "  ";
    }
    result += "- " + vertex._id + "   " + stringifyPath(path);
    console.log(result);
  };

  var uniqueness = { vertices : ArangoTraverser.UNIQUE_NONE, edges : ArangoTraverser.UNIQUE_NONE };

  db.tusers.load();
  db.trelations.load();
  var traverser = new ArangoTraverser("trelations", ArangoTraverser.DEPTH_FIRST, ArangoTraverser.PRE_ORDER, uniqueness, visitor);
  traverser.traverse("tusers/claudius", context);
  console.log("------------");
  var traverser = new ArangoTraverser("trelations", ArangoTraverser.DEPTH_FIRST, ArangoTraverser.POST_ORDER, uniqueness, visitor);
  traverser.traverse("tusers/claudius", context);
  console.log("------------");
  var traverser = new ArangoTraverser("trelations", ArangoTraverser.BREADTH_FIRST, ArangoTraverser.PRE_ORDER, uniqueness, visitor);
  traverser.traverse("tusers/claudius", context);
  console.log("------------");
  var traverser = new ArangoTraverser("trelations", ArangoTraverser.BREADTH_FIRST, ArangoTraverser.POST_ORDER, uniqueness, visitor);
  traverser.traverse("tusers/claudius", context);

}());

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
/*
exports.ArangoTraverser        = ArangoTraverser;

exports.ArangoIncludeAllFilter = IncludeAllFilter;
exports.ArangoOutboundExpander = OutboundExpander;
exports.ArangoInboundExpander  = InboundExpander;
*/
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
