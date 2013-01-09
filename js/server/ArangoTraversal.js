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
                            visitor, 
                            filter, 
                            expander) {
    if (typeof edgeCollection === "string") {
      edgeCollection = internal.db._collection(edgeCollection);
    }
    
    if (! (edgeCollection instanceof ArangoCollection) ||
        edgeCollection.type() != internal.ArangoCollection.TYPE_EDGE) {
      throw "invalid edgeCollection";
    }

    this._visitor            = visitor;
    this._filter             = filter || IncludeAllFilter;
    this._expander           = expander || OutboundExpander;
    this._edgeCollection     = edgeCollection;
    
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

  ArangoTraverser.prototype.traverse = function (startVertex, 
                                                 visitationStrategy, 
                                                 uniqueness,
                                                 context, 
                                                 state) {
    // check the start vertex
    if (startVertex == undefined) {
      throw "invalid startVertex specified for traversal";
    }
    if (typeof startVertex == "string") {
      startVertex = internal.db._document(startVertex);
    }

    // check the visitation strategy    
    if (visitationStrategy !== ArangoTraverser.BREADTH_FIRST &&
        visitationStrategy !== ArangoTraverser.DEPTH_FIRST) {
      throw "invalid visitationStrategy";
    }

    // set user defined context & state
    this._context = context;
    this._state   = state;
    
   
    // initialise the traversal strategy 
    var levelState = InitLevelState(startVertex, function () {
      if (visitationStrategy === ArangoTraverser.BREADTH_FIRST) {
        return BreadthFirstSearch();
      }

      return DepthFirstSearch();
    }()); 
    
    // initialise uniqueness check attributes
    var uniquenessCheck = { 
      vertices: uniqueness.vertices || ArangoTraverser.UNIQUE_NONE,
      edges:    uniqueness.edges || ArangoTraverser.UNIQUE_NONE
    };
    

    // now execute the actual traversal...
    // iterate until we find the end 
    while (true) {
      var currentElement = levelState.currentElement();
      var edge           = currentElement.edge; 
      var vertex         = currentElement.vertex;
      
      if (! ProcessVertex(this, vertex, levelState)) {
        // stop traversal because PRUNE was returned
        return;
      }
     
      if (! levelState.next(levelState)) {
        // we're done
        return; 
      }
    }
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
/// @brief initialise some internal state for the traversal
////////////////////////////////////////////////////////////////////////////////
     
  function InitLevelState (startVertex, strategy) {
    return { 
      level:          0,
      maxLevel:       0,
      index:          0,
      stack:          [ [ { edge: null, vertex: startVertex, parentIndex: 0 } ] ],
      path:           { edges: [ ], vertices: [ startVertex ] },
      visited:        { edges: { }, vertices: { } },

      next:           function () {
        return strategy.next(this);
      },
      
      currentElement: function () {
        var currentElement = strategy.currentElement(this);
        strategy.assemblePath(this, currentElement);

        return currentElement;
      },

      elementAt:      function (searchLevel, searchIndex) {
        return this.stack[searchLevel][searchIndex];
      },
      
      numElements:    function () {
        return this.stack[this.level].length;
      },

      isUniqueVertex: function (checkMode, vertex) {
        return this.isUnique(checkMode, this.visited.vertices, vertex._id);
      },
      
      isUniqueEdge: function (checkMode, edge) {
        return this.isUnique(checkMode, this.visited.edges, edge._id);
      },

      isUnique: function (checkMode, elementsSeen, element) {
        if (checkMode === ArangoTraverser.UNIQUE_NONE) {
          return true;
        }

        var seen = elementsSeen[element] || false;
        if (! seen) {
          // now mark the element as being visited
          elementsSeen[element] = true;
        }

        return ! seen;
      }

    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for breadth-first strategy
////////////////////////////////////////////////////////////////////////////////

  function BreadthFirstSearch () {
    return {
      // TODO: ADD UNIQUENESS CHECK HERE
      next: function (levelState) {
        if (++levelState.index >= levelState.numElements()) {
          levelState.index = 0;
          if (++levelState.level > levelState.maxLevel) {
            // we're done
            return false;
          }
        }

        return true;
      },

      currentElement : function (levelState) {
        return levelState.elementAt(levelState.level, levelState.index);
      },

      assemblePath: function (levelState, currentElement) {
        var path            = levelState.path;

        for (var i = levelState.level; i > 0; --i) {
          path.edges[i - 1] = currentElement.edge;
          path.vertices[i]  = currentElement.vertex;

          currentElement    = levelState.elementAt(i - 1, currentElement.parentIndex);
        }
      }
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for depth-first strategy
////////////////////////////////////////////////////////////////////////////////

  function DepthFirstSearch () {
    return {
      // TODO: ADD UNIQUENESS CHECK HERE
      next: function (levelState) {
        while (levelState.level < levelState.maxLevel) {
          // try going into children first
          var nextStack = levelState.stack[levelState.level + 1];

          if (nextStack.length == 0) {
            // nothing to recurse into
            break;
          }

          var nextElement = nextStack[nextStack.length - 1];
          if (nextElement.edge === null) {
            // nothing to recurse into
            break;
          }

          ++levelState.level;
        }

        while (levelState.numElements() == 0) { 
          if (levelState.level == 0) {
            // we're done
            return false;
          }

          if (levelState.level > 0) {
            var edge = levelState.path.edges.pop();
            delete levelState.visited.edges[edge._id];
          }

          var vertex = levelState.path.vertices.pop();
          delete levelState.visited.vertices[vertex._id];

          --levelState.level;
          --levelState.maxLevel;
        }

        return true;     
      },
      
      currentElement : function (levelState) {
        var currentElement = levelState.stack[levelState.level].pop();
        return currentElement;
      },

      assemblePath: function (levelState, currentElement) {
        if (levelState.level > 0) {
          levelState.path.edges[levelState.level - 1] = currentElement.edge;
        }
        levelState.path.vertices[levelState.level] = currentElement.vertex;
      }

    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function that is executed for each vertex found
///
/// this function is applying the filter criteria and calls the visitor 
/// function for the vertex if it should be included in the traversal.
/// it will then also fetch the connected edges for the vertex.
////////////////////////////////////////////////////////////////////////////////

  function ProcessVertex (traverser, vertex, levelState) {
    // check filter condition for vertex
    var filterResult = traverser._filter(traverser, vertex, levelState.path);

    if (filterResult === ArangoTraverser.INCLUDE_AND_CONTINUE || 
        filterResult === ArangoTraverser.INCLUDE_AND_PRUNE) {
      // vertex should be included, so visit it
      traverser._visitor(traverser, vertex, levelState.path);

      // get connected edges for vertex
      traverser._expander(traverser, vertex, levelState.path).forEach(function (connection) {
        var nextLevel = levelState.level + 1;

        if (levelState.stack[nextLevel] == undefined) {
          levelState.stack[nextLevel] = [ ];
        }

        if (levelState.stack[nextLevel].length == 0) {
          ++levelState.maxLevel;
        }

        connection.parentIndex = levelState.index;
        levelState.stack[nextLevel].push(connection); 
      });
    }

    // return true only if we should continue
    return (filterResult === ArangoTraverser.INCLUDE_AND_CONTINUE || 
            filterResult === ArangoTraverser.EXCLUDE_AND_CONTINUE);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief default filter function (if none specified)
///
/// this filter will simply include every vertex
////////////////////////////////////////////////////////////////////////////////

  function IncludeAllFilter (traverser, vertex, path) {
    return ArangoTraverser.INCLUDE_AND_CONTINUE;
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
  
  ArangoTraverser.UNIQUE_NONE          = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief visitation strategy breadth first
////////////////////////////////////////////////////////////////////////////////
  
  ArangoTraverser.BREADTH_FIRST        = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief visitation strategy depth first
////////////////////////////////////////////////////////////////////////////////

  ArangoTraverser.DEPTH_FIRST          = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief include this node in the result and continue the traversal
////////////////////////////////////////////////////////////////////////////////

  ArangoTraverser.INCLUDE_AND_CONTINUE = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief include this node in the result, but don’t continue the traversal
////////////////////////////////////////////////////////////////////////////////

  ArangoTraverser.INCLUDE_AND_PRUNE    = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief exclude this node from the result, but continue the traversal
////////////////////////////////////////////////////////////////////////////////

  ArangoTraverser.EXCLUDE_AND_CONTIUE  = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief exclude this node from the result and don’t continue the traversal
////////////////////////////////////////////////////////////////////////////////

  ArangoTraverser.EXCLUDE_AND_PRUNE    = 3;

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
  var state   = { someState   : { } };

  var visitor = function (traverser, vertex, path) {
    var result = "";
    for (var i = 0; i < path.edges.length; ++i) {
      result += "  ";
    }
    result += "- " + vertex._id;
    console.log(result);
  };

  var uniqueness = { vertices : ArangoTraverser.UNIQUE_GLOBAL, edges : ArangoTraverser.UNIQUE_NONE };

  db.tusers.load();
  db.trelations.load();

  var traverser = new ArangoTraverser("trelations", visitor);

  traverser.traverse("tusers/claudius", ArangoTraverser.DEPTH_FIRST, uniqueness, context, state);
  console.log("------------");
  traverser.traverse("tusers/claudius", ArangoTraverser.BREADTH_FIRST, uniqueness, context, state);

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
