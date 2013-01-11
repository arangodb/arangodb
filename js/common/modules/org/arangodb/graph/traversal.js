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

function ArangoTraverser (edgeCollection, properties) {
  // check edgeCollection
  if (edgeCollection == undefined || edgeCollection == null) {
    throw "invalid edgeCollection";
  }

  this._edgeCollection = edgeCollection;
  
  if (typeof properties !== "object") {
    throw "invalid properties";
  }

  this._order = properties.visitOrder || ArangoTraverser.PRE_ORDER;
  this._itemOrder = properties.itemOrder || ArangoTraverser.FORWARD;

  // check the visitation strategy    
  if (properties.strategy !== ArangoTraverser.BREADTH_FIRST &&
      properties.strategy !== ArangoTraverser.DEPTH_FIRST) {
    throw "invalid strategy";
  }
  this._strategy = properties.strategy;

  // initialise uniqueness check attributes
  this._uniqueness = { 
    vertices: properties.uniqueness.vertices || ArangoTraverser.UNIQUE_NONE,
    edges:    properties.uniqueness.edges    || ArangoTraverser.UNIQUE_NONE
  };

  // callbacks
  // visitor
  this._visitor = properties.visitor;

  // filter
  this._filter  = properties.filter || function () {
    return {
      visit:  true,
      expand: true
    };
  };

  this._expander = properties.expander || OutboundExpander;

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
  if (startVertex == undefined || startVertex == null) {
    throw "invalid startVertex specified for traversal";
  }

  // set user defined context
  this._context = context;

  // run the traversal
  var strategy;
  if (this._strategy === ArangoTraverser.BREADTH_FIRST) {
    strategy = BreadthFirstSearch;
  }
  else {
    strategy = DepthFirstSearch;
  }

  strategy().run(this, startVertex);
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
/// @brief check if we must process items in reverse order
////////////////////////////////////////////////////////////////////////////////

function CheckReverse (traverser) {
  if (traverser._order === ArangoTraverser.POST_ORDER) {
    // post order
    if (traverser._itemOrder === ArangoTraverser.FORWARD) {
      return true;
    }
  }
  else if (traverser._order === ArangoTraverser.PRE_ORDER) {
    // pre order
    if (traverser._itemOrder === ArangoTraverser.BACKWARD && traverser._strategy === ArangoTraverser.BREADTH_FIRST) {
      return true;
    }
    if (traverser._itemOrder === ArangoTraverser.FORWARD && traverser._strategy === ArangoTraverser.DEPTH_FIRST) {
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

    run: function (traverser, startVertex) {
      var toVisit = [ { edge: null, vertex: startVertex, parentIndex: -1 } ];
      var visited = { edges: { }, vertices: { } };

      var index = 0;
      var step = 1;
      var reverse = CheckReverse(traverser);

      while ((step == 1 && index < toVisit.length) ||
             (step == -1 && index >= 0)) {
        var current = toVisit[index];
        var vertex  = current.vertex;
        var edge    = current.edge;

        if (current.visit == null) {
          current.visit = false;
          
          // first apply uniqueness check
          if (! CheckUniqueness(traverser._uniqueness, visited, vertex, edge)) {
            if (index < toVisit.length - 1) {
              index += step;
            }
            else {
              step = -1;
            }
            continue;
          }

          var path = this.createPath(toVisit, index);

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
              reverse && connected.reverse();
              for (var i = 0; i < connected.length; ++i) {
                connected[i].parentIndex = index;
                toVisit.push(connected[i]);
              }
            }
          }
            
          if (traverser._order === ArangoTraverser.POST_ORDER) {
            if (index < toVisit.length - 1) {
              index += step;
            }
            else {
              step = -1;
            }
          }
        }
        else {
          if (traverser._order === ArangoTraverser.POST_ORDER && current.visit) {
            var path = this.createPath(toVisit, index);
            traverser._visitor(traverser, vertex, path);
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

    run: function (traverser, startVertex) {
      var toVisit = [ { edge: null, vertex: startVertex, visit: null } ];
      var path    = { edges: [ ], vertices: [ ] };
      var visited = { edges: { }, vertices: { } };
      var reverse = CheckReverse(traverser);

      while (toVisit.length > 0) {
        // peek at the top of the stack
        var current = toVisit[toVisit.length - 1];
        var vertex  = current.vertex;
        var edge    = current.edge;

        // check if we visit the element for the first time
        if (current.visit == null) {
          current.visit = false;

          // first apply uniqueness check
          if (! CheckUniqueness(traverser._uniqueness, visited, vertex, edge)) {
            // skip element if not unique
            toVisit.pop();
            continue;
          }

          // push the current element onto the path stack
          if (edge != null) {
            path.edges.push(edge);
          }
          path.vertices.push(vertex);

          
          var filterResult = traverser._filter(traverser, vertex, path);
          if (traverser._order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
            // preorder visit
            traverser._visitor(traverser, vertex, path);
          }
          else {
            // postorder. mark the element visitation flag because we'll got to check it later
            current.visit = filterResult.visit || false;
          }

          // expand the element's children?
          if (filterResult.expand) {
            var connected = traverser._expander(traverser, vertex, path);
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
          if (traverser._order === ArangoTraverser.POST_ORDER && current.visit) {
            // postorder visitation
            traverser._visitor(traverser, vertex, path);
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoTraverser
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.Traverser        = ArangoTraverser;
exports.OutboundExpander = OutboundExpander;
exports.InboundExpander  = InboundExpander;
  
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
