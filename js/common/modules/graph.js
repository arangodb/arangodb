/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         plusplus: true */
/*global require, WeakDictionary, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal"),
  db = internal.db,
  ArangoCollection = internal.ArangoCollection,
  findOrCreateCollectionByName,
  findOrCreateEdgeCollectionByName;

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

findOrCreateCollectionByName = function (name) {
  var col = internal.db._collection(name);

  if (col === null) {
    col = internal.db._create(name);
  } else if (!(col instanceof ArangoCollection) || col.type() != ArangoCollection.TYPE_DOCUMENT) {
    throw "<" + name + "> must be a document collection";
  }

  if (col === null) {
    throw "collection '" + name + "' has vanished";
  }

  return col;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create an edge collection by name
////////////////////////////////////////////////////////////////////////////////

findOrCreateEdgeCollectionByName = function (name) {
  var col = internal.db._collection(name);

  if (col === null) {
    col = internal.db._createEdgeCollection(name);
  } else if (!(col instanceof ArangoCollection) || col.type() != ArangoCollection.TYPE_EDGE) {
    throw "<" + name + "> must be an edge collection";
  }

  if (col === null) {
    throw "collection '" + name + "' has vanished";
  }

  return col;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                              Edge
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new edge object
////////////////////////////////////////////////////////////////////////////////

function Edge(graph, id) {
  var properties = graph._edges.document(id);

  this._graph = graph;
  this._id = id;

  if (properties) {
    this._properties = properties;
  } else {
    throw "accessing a deleted edge";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of an edge
///
/// @FUN{@FA{edge}.getId()}
///
/// Returns the identifier of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-id
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getId = function () {
  return this._properties.$id;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the to vertex
///
/// @FUN{@FA{edge}.getInVertex()}
///
/// Returns the vertex at the head of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-in-vertex
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getInVertex = function () {
  return this._graph.constructVertex(this._properties._to);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief label of an edge
///
/// @FUN{@FA{edge}.getLabel()}
///
/// Returns the label of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-label
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getLabel = function () {
  return this._properties.$label;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the from vertex
///
/// @FUN{@FA{edge}.getOutVertex()}
///
/// Returns the vertex at the tail of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-out-vertex
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getOutVertex = function () {
  return this._graph.constructVertex(this._properties._from);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of an edge
///
/// @FUN{@FA{edge}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} an @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-property
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of an edge
///
/// @FUN{@FA{edge}.getPropertyKeys()}
///
/// Returns all propety names an @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-property-keys
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of an edge
///
/// @FUN{@FA{edge}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} an @FA{edges} to @FA{value}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-set-property
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.setProperty = function (name, value) {
  var shallow = this._properties.shallowCopy,
    id;

  // Could potentially change the weight of edges
  this._graph.emptyCachedPredecessors();

  shallow.$id = this._properties.$id;
  shallow.$label = this._properties.$label;
  shallow[name] = value;

  // TODO use "update" if this becomes available
  id = this._graph._edges.replace(this._properties, shallow);
  this._properties = this._graph._edges.document(id);

  return value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of an edge
///
/// @FUN{@FA{edge}.properties()}
///
/// Returns all properties and their values of an @FA{edge}
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-properties
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.properties = function () {
  return this._properties.shallowCopy;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief edge printing
////////////////////////////////////////////////////////////////////////////////

Edge.prototype._PRINT = function (seen, path, names) {
  // Ignores the standard arguments
  seen = path = names = null;

  if (!this._id) {
    internal.output("[deleted Edge]");
  } else if (this._properties.$id !== undefined) {
    if (typeof this._properties.$id === "string") {
      internal.output("Edge(\"", this._properties.$id, "\")");
    } else {
      internal.output("Edge(", this._properties.$id, ")");
    }
  } else {
    internal.output("Edge(<", this._id, ">)");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            VERTEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new vertex object
////////////////////////////////////////////////////////////////////////////////

function Vertex(graph, id) {
  var properties = graph._vertices.document(id);

  this._graph = graph;
  this._id = id;

  if (properties) {
    this._properties = properties;
  } else {
    throw "accessing a deleted edge";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an inbound edge
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{id})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} and returns the edge
/// object. The identifier @FA{id} must be a unique identifier or null.
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{id}, @FA{label})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} with given label and
/// returns the edge object.
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{id}, @FA{label}, @FA{data})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} with given label and
/// properties defined in @FA{data}. Returns the edge object.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-add-in-edge
///
/// @verbinclude graph-vertex-add-in-edge2
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addInEdge = function (out, id, label, data) {
  return this._graph.addEdge(out, this, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an outbound edge
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} and returns the edge
/// object.
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// returns the edge object.
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label}, @FA{data})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// properties defined in @FA{data}. Returns the edge object.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-add-out-edge
///
/// @verbinclude graph-vertex-add-out-edge2
///
/// @verbinclude graph-vertex-add-out-edge3
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addOutEdge = function (ine, id, label, data) {
  return this._graph.addEdge(this, ine, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound and outbound edges
///
/// @FUN{@FA{vertex}.edges()}
///
/// Returns a list of in- or outbound edges of the @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.edges = function () {
  var graph = this._graph,
    query = graph._edges.edges(this._id);

  return query.map(function (result) {
    return graph.constructEdge(result._id);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of a vertex
///
/// @FUN{@FA{vertex}.getId()}
///
/// Returns the identifier of the @FA{vertex}. If the vertex was deleted, then
/// @LIT{undefined} is returned.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-id
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getId = function () {
  return this._properties.$id;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges with given label
///
/// @FUN{@FA{vertex}.getInEdges(@FA{label}, ...)}
///
/// Returns a list of inbound edges of the @FA{vertex} with given label(s).
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-in-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getInEdges = function () {
  var labels = Array.prototype.slice.call(arguments),
    result = this.inbound();

  if (labels.length > 0) {
    result = result.filter(function (edge) {
      return (labels.lastIndexOf(edge.getLabel()) > -1);
    });
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges with given label
///
/// @FUN{@FA{vertex}.getOutEdges(@FA{label}, ...)}
///
/// Returns a list of outbound edges of the @FA{vertex} with given label(s).
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-out-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getOutEdges = function () {
  var labels = Array.prototype.slice.call(arguments),
    result = this.outbound();

  if (labels.length > 0) {
    result = result.filter(function (edge) {
      return (labels.lastIndexOf(edge.getLabel()) > -1);
    });
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of a vertex
///
/// @FUN{@FA{vertex}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} a @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of a vertex
///
/// @FUN{@FA{vertex}.getPropertyKeys()}
///
/// Returns all propety names a @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property-keys
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges
///
/// @FUN{@FA{vertex}.inbound()}
///
/// Returns a list of inbound edges of the @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-inbound
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inbound = function () {
  var graph = this._graph;

  return graph._edges.inEdges(this._id).map(function (result) {
    return graph.constructEdge(result._id);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges
///
/// @FUN{@FA{vertex}.outbound()}
///
/// Returns a list of outbound edges of the @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-outbound
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outbound = function () {
  var graph = this._graph;

  return graph._edges.outEdges(this._id).map(function (result) {
    return graph.constructEdge(result._id);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of a vertex
///
/// @FUN{@FA{vertex}.properties()}
///
/// Returns all properties and their values of a @FA{vertex}
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-properties
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.properties = function () {
  return this._properties.shallowCopy;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of a vertex
///
/// @FUN{@FA{vertex}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} a @FA{vertex} to @FA{value}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-set-property
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.setProperty = function (name, value) {
  var shallow = this._properties.shallowCopy,
    id;

  shallow.$id = this._properties.$id;
  shallow[name] = value;

  // TODO use "update" if this becomes available
  id = this._graph._vertices.replace(this._id, shallow);
  this._properties = this._graph._vertices.document(id);

  return value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of common neighbors
///
/// @FUN{@FA{vertex}.commonNeighborsWith(@FA{target_vertex}, @FA{options})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.commonNeighborsWith = function (target_vertex, options) {
  var neighbor_set_one,
    neighbor_set_two,
    id_only,
    common_neighbors,
    all_neighbors,
    return_value;

  options = options || {};

  id_only = function (neighbor) {
    return neighbor.id;
  };

  if (typeof(target_vertex) != 'object') {
    throw "<target_vertex> must be a vertex object";
  }

  neighbor_set_one = this.getNeighbors(options).map(id_only);
  neighbor_set_two = target_vertex.getNeighbors(options).map(id_only);

  common_neighbors = neighbor_set_one.intersect(neighbor_set_two);

  if ((options.listed !== undefined) && (options.listed === true)) {
    return_value = common_neighbors;
  } else if ((options.normalized !== undefined) && (options.normalized === true)) {
    all_neighbors = neighbor_set_one.unite(neighbor_set_two);
    return_value = (common_neighbors.length / all_neighbors.length);
  } else {
    return_value = common_neighbors.length;
  }

  return return_value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of common properties
///
/// @FUN{@FA{vertex}.commonPropertiesWith(@FA{target_vertex}, @FA{options})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.commonPropertiesWith = function (other_vertex, options) {
  var property_names,
    shared_properties = [],
    this_vertex = this,
    return_value;

  options = options || {};

  property_names = this_vertex.getPropertyKeys().unite(other_vertex.getPropertyKeys());

  property_names.forEach(function (property) {
    if (this_vertex.getProperty(property) === other_vertex.getProperty(property)) {
      shared_properties.push(property);
    }
  });

  if ((options.listed !== undefined) && (options.listed === true)) {
    return_value = shared_properties;
  } else if ((options.normalized !== undefined) && (options.normalized === true)) {
    return_value = shared_properties.length / property_names.length;
  } else {
    return_value = shared_properties.length;
  }

  return return_value;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shortest path to a certain vertex, return the ID
///
/// @FUN{@FA{vertex}.pathTo(@FA{target_vertex}, @FA{options})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.pathTo = function (target_vertex, options) {
  if (typeof(target_vertex) != 'object') {
    throw "<target_vertex> must be an object";
  }
  var predecessors = target_vertex.determinePredecessors(this, options || {});
  return (predecessors ? target_vertex.pathesForTree(predecessors) : []);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shortest path and return the number of edges to the target vertex
///
/// @FUN{@FA{vertex}.distanceTo(@FA{target_vertex}, @FA{options})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.distanceTo = function (target_vertex, options) {
  var predecessors = target_vertex.determinePredecessors(this, options || {}),
    current_vertex_id = target_vertex.getId(),
    count = 0;

  while (predecessors[current_vertex_id] !== undefined) {
    current_vertex_id = predecessors[current_vertex_id][0];
    count += 1;
  }

  if (current_vertex_id !== this.getId()) {
    count = Infinity;
  }

  return count;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief determine all the pathes to this node from source
///
/// @FUN{@FA{vertex}.determinePredecessors(@FA{source}, @FA{options})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.determinePredecessors = function (source, options) {
  var graph = this._graph,      // Graph
    determined_list = [],       // [ID]
    predecessors,               // { ID => [ID] }
    source_id = source.getId(), // ID
    todo_list = [source_id],    // [ID]
    distances = {},             // { ID => Number }
    current_vertex,             // Vertex
    current_vertex_id,          // ID
    return_value = false;       // { ID => [ID]}
  distances[source_id] = 0;

  if (options.cached) {
    predecessors = graph.getCachedPredecessors(this, source);
  }

  if (!predecessors) {
    predecessors = {};
    while (todo_list.length > 0) {
      current_vertex_id = this._getShortestDistance(todo_list, distances);
      current_vertex = this._graph.getVertex(current_vertex_id);

      if (current_vertex_id === this.getId()) {
        return_value = predecessors;
        break;
      } else {
        todo_list.removeLastOccurrenceOf(current_vertex_id);
        determined_list.push(current_vertex_id);

        todo_list = todo_list.unite(current_vertex._processNeighbors(
          determined_list,
          distances,
          predecessors,
          options
        ));
      }
    }
    graph.setCachedPredecessors(this, source, predecessors);
  }

  return return_value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function for determinePredecessors (changes distance and predecessors
///
/// @FUN{@FA{vertex}._processNeighbors(@FA{determined}, @FA{distances}, @FA{predecessors})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype._processNeighbors = function (determined_list, distances, predecessors, options) {
  var i,
    current_neighbor_id,
    current_distance,
    raw_neighborlist,
    compared_distance,
    current_weight,
    not_determined_neighbors = [];

  raw_neighborlist = this.getNeighbors(options);

  for (i = 0; i < raw_neighborlist.length; i += 1) {
    current_neighbor_id = raw_neighborlist[i].id;

    if (determined_list.lastIndexOf(current_neighbor_id) === -1) {
      current_weight = raw_neighborlist[i].weight;
      current_distance = distances[this.getId()] + current_weight;

      not_determined_neighbors.push(current_neighbor_id);

      compared_distance = distances[current_neighbor_id];
      if ((compared_distance === undefined) || (compared_distance > current_distance)) {
        predecessors[current_neighbor_id] = [this.getId()];
        distances[current_neighbor_id] = current_distance;
      } else if (compared_distance === current_distance) {
        predecessors[current_neighbor_id].push(this.getId());
      }
    }
  }

  return not_determined_neighbors;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get all paths from root to leave vertices for a given tree
///
/// @FUN{@FA{vertex}.pathesForTree(@FA{tree}, @FA{path_to_here})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.pathesForTree = function (tree, path_to_here) {
  var my_children = tree[this.getId()],
    i,
    my_child,
    pathes = [];

  path_to_here = path_to_here || [];
  path_to_here = path_to_here.concat(this.getId());

  if (my_children === undefined) {
    pathes = [path_to_here.reverse()];
  } else {
    for (i = 0; i < my_children.length; i += 1) {
      my_child = this._graph.getVertex(my_children[i]);
      pathes = pathes.concat(my_child.pathesForTree(tree, path_to_here));
    }
  }

  return pathes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get all neighbours for this vertex
///
/// @FUN{@FA{vertex}.getNeighbors(@FA{options})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getNeighbors = function (options) {
  var current_vertex,
    target_array = [],
    addNeighborToList;

  if (! options) {
    options = { };
  }

  var direction = options.direction || 'both',
    labels = options.labels,
    weight = options.weight,
    weight_function = options.weight_function,
    default_weight = options.default_weight || Infinity,
    only = options.only;

  addNeighborToList = function (current_edge, current_vertex) {
    var neighbor_info, current_label = current_edge.getLabel();

    if ((labels === undefined) || (labels.lastIndexOf(current_label) > -1)) {
      neighbor_info = { id: current_vertex.getId() };
      if (weight !== undefined) {
        neighbor_info.weight = current_edge.getProperty(weight) || default_weight;
      } else if (weight_function !== undefined) {
        neighbor_info.weight = weight_function(current_edge);
      } else {
        neighbor_info.weight = 1;
      }

      if ((only === undefined) || (only(current_edge))) {
        target_array.push(neighbor_info);
      }
    }
  };

  if ((direction === 'both') || (direction === 'outbound')) {
    this.getOutEdges().forEach(function (current_edge) {
      current_vertex = current_edge.getInVertex();
      addNeighborToList(current_edge, current_vertex);
    });
  }

  if ((direction === 'both') || (direction === 'inbound')) {
    this.getInEdges().forEach(function (current_edge) {
      current_vertex = current_edge.getOutVertex();
      addNeighborToList(current_edge, current_vertex);
    });
  }

  return target_array;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the shortest distance for a given list of vertices and
/// their distances
///
/// @FUN{@FA{vertex}._getShortestDistance(@FA{todo_list}, @FA{distances})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype._getShortestDistance = function (todo_list, distances) {
  var shortest_distance = Infinity,
    vertex = null,
    i,
    distance;

  for (i = 0; i < todo_list.length; i += 1) {
    distance = distances[todo_list[i]];
    if (distance < shortest_distance) {
      shortest_distance = distance;
      vertex = todo_list[i];
    }
  }

  return vertex;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief number of edges
///
/// @FUN{@FA{vertex}.degree()}
///
/// Returns the number of edges of the @FA{vertex}.
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.degree = function () {
  return this._graph._edges.edges(this._id).length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief number of in-edges
///
/// @FUN{@FA{vertex}.inDegree()}
///
/// Returns the number of inbound edges of the @FA{vertex}.
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inDegree = function () {
  return this._graph._edges.inEdges(this._id).length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief number of out-edges
///
/// @FUN{@FA{vertex}.outDegree()}
///
/// Returns the number of outbound edges of the @FA{vertex}.
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outDegree = function () {
  return this._graph._edges.outEdges(this._id).length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate a measurement
///
/// @FUN{@FA{vertex}.measurement(@FA{measurement})}
///
/// Calculates the eccentricity, betweenness or closeness of the vertex
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.measurement = function (measurement) {
  var graph = this._graph,
    source = this,
    value;

  if (measurement === "betweenness") {
    value = graph.geodesics({
      grouped: true,
      threshold: true
    }).reduce(function (count, geodesic_group) {
      var included = geodesic_group.filter(function (geodesic) {
        return geodesic.slice(1, -1).indexOf(source.getId()) > -1;
      });

      return (included ? count + (included.length / geodesic_group.length) : count);
    }, 0);
  } else if (measurement === "eccentricity") {
    value = graph._vertices.toArray().reduce(function (calculated, target) {
      var distance = source.distanceTo(graph.getVertex(target._id));
      return Math.max(calculated, distance);
    }, 0);
  } else if (measurement === "closeness") {
    value = graph._vertices.toArray().reduce(function (calculated, target) {
      var distance = source.distanceTo(graph.getVertex(target._id));
      return calculated + distance;
    }, 0);
  } else {
    throw "Unknown Measurement '" + measurement + "'";
  }

  return value;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex representation
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype._PRINT = function (seen, path, names) {
  // Ignores the standard arguments
  seen = path = names = null;

  if (!this._id) {
    internal.output("[deleted Vertex]");
  } else if (this._properties.$id !== undefined) {
    if (typeof this._properties.$id === "string") {
      internal.output("Vertex(\"", this._properties.$id, "\")");
    } else {
      internal.output("Vertex(", this._properties.$id, ")");
    }
  } else {
    internal.output("Vertex(<", this._id, ">)");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             GRAPH
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new graph object
///
/// @FUN{Graph(@FA{name}, @FA{vertices}, @FA{edges})}
///
/// Constructs a new graph object using the collection @FA{vertices} for all
/// vertices and the collection @FA{edges} for all edges. Note that it is
/// possible to construct two graphs with the same vertex set, but different
/// edge sets.
///
/// @FUN{Graph(@FA{name})}
///
/// Returns a known graph.
///
/// @EXAMPLES
///
/// @verbinclude graph-constructor
////////////////////////////////////////////////////////////////////////////////

function Graph(name, vertices, edges) {
  var gdb = internal.db._collection("_graphs"),
    graphProperties,
    graphPropertiesId;

  if (gdb === null) {
    throw "_graphs collection does not exist.";
  }

  if (typeof name !== "string" || name === "") {
    throw "<name> must be a string";
  }

  if (vertices === undefined && edges === undefined) {
    // Find an existing graph

    graphProperties = gdb.firstExample('name', name);

    if (graphProperties === null) {
      try {
        graphProperties = gdb.document(name);
      } catch (e) {
        throw "no graph named '" + name + "' found";
      }

      if (graphProperties === null) {
        throw "no graph named '" + name + "' found";
      }
    }

    vertices = internal.db._collection(graphProperties.vertices);

    if (vertices === null) {
      throw "vertex collection '" + graphProperties.vertices + "' has vanished";
    }

    edges = internal.db._collection(graphProperties.edges);

    if (edges === null) {
      throw "edge collection '" + graphProperties.edges + "' has vanished";
    }
  } else if (typeof vertices !== "string" || vertices === "") {
    throw "<vertices> must be a string or null";
  } else if (typeof edges !== "string" || edges === "") {
    throw "<edges> must be a string or null";
  } else {

    // Create a new graph or get an existing graph
    vertices = findOrCreateCollectionByName(vertices);
    edges = findOrCreateEdgeCollectionByName(edges);

    // Currently buggy?
    edges.ensureUniqueConstraint("$id");
    vertices.ensureUniqueConstraint("$id");

    graphProperties = gdb.firstExample('name', name);

    // Graph doesn't exist yet
    if (graphProperties === null) {

      // check if know that graph
      graphProperties = gdb.firstExample('vertices',
        vertices._id,
        'edges',
        edges._id
        );

      if (graphProperties === null) {
        graphPropertiesId = gdb.save({ 'vertices' : vertices._id,
                       'verticesName' : vertices.name(),
                       'edges' : edges._id,
                       'edgesName' : edges.name(),
                       'name' : name });

        graphProperties = gdb.document(graphPropertiesId);
      } else {
        throw "found graph but has different <name>";
      }
    } else {
      if (graphProperties.vertices !== vertices._id) {
        throw "found graph but has different <vertices>";
      }

      if (graphProperties.edges !== edges._id) {
        throw "found graph but has different <edges>";
      }
    }
  }

  this._properties = graphProperties;

  // and store the collections
  this._vertices = vertices;
  this._edges = edges;

  // and weak dictionary for vertices and edges
  this._weakVertices = new WeakDictionary();
  this._weakEdges = new WeakDictionary();

  // and store the cashes
  this.predecessors = {};
  this.distances = {};
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the graph, the vertices, and the edges
///
/// @FUN{@FA{graph}.drop()}
///
/// Drops the graph, the vertices, and the edges. Handle with care.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.drop = function () {
  var gdb = internal.db._collection("_graphs");

  gdb.remove(this._properties);

  this._vertices.drop();
  this._edges.drop();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an edge to the graph
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id})}
///
/// Creates a new edge from @FA{out} to @FA{in} and returns the edge object. The
/// identifier @FA{id} must be a unique identifier or null.
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id}, @FA{label})}
///
/// Creates a new edge from @FA{out} to @FA{in} with @FA{label} and returns the
/// edge object.
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge contains the
/// properties defined in @FA{data}.
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id}, @FA{label}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge has the
/// label @FA{label} and contains the properties defined in @FA{data}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-add-edge
///
/// @verbinclude graph-graph-add-edge2
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addEdge = function (out_vertex, in_vertex, id, label, data) {
  var ref,
    shallow;

  this.emptyCachedPredecessors();

  if (typeof label === 'object') {
    data = label;
    label = undefined;
  }

  if (label === undefined) {
    label = null;
  }

  if (data === null || typeof data !== "object") {
    shallow = {};
  }
  else {
    shallow = data.shallowCopy || {};
  }

  shallow.$id = id || null;
  shallow.$label = label || null;

  ref = this._edges.save(out_vertex._id, in_vertex._id, shallow);

  return this.constructEdge(ref._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a vertex to the graph
///
/// @FUN{@FA{graph}.addVertex(@FA{id})}
///
/// Creates a new vertex and returns the vertex object. The identifier
/// @FA{id} must be a unique identifier or null.
///
/// @FUN{@FA{graph}.addVertex(@FA{id}, @FA{data})}
///
/// Creates a new vertex and returns the vertex object. The vertex contains
/// the properties defined in @FA{data}.
///
/// @EXAMPLES
///
/// Without any properties:
///
/// @verbinclude graph-graph-add-vertex
///
/// With given properties:
///
/// @verbinclude graph-graph-add-vertex2
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addVertex = function (id, data) {
  var ref,
    shallow;

  if (data === null || typeof data !== "object") {
    shallow = {};
  }
  else {
    shallow = data.shallowCopy || {};
  }

  shallow.$id = id || null;

  ref = this._vertices.save(shallow);

  return this.constructVertex(ref._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a vertex given its id
///
/// @FUN{@FA{graph}.getVertex(@FA{id})}
///
/// Returns the vertex identified by @FA{id} or @LIT{null}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertex = function (id) {
  var ref,
    vertex;

  ref = this._vertices.firstExample('$id', id);

  if (ref !== null) {
    vertex = this.constructVertex(ref._id);
  } else {
    try {
      vertex = this.constructVertex(id);
    } catch (e) {
      vertex = null;
    }
  }

  return vertex;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get a vertex from the graph, create it if it doesn't exist
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getOrAddVertex = function (id) {
  var v = this.getVertex(id);
  if (v === null) {
    v = this.addVertex(id);
  }
  return v;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all vertices
///
/// @FUN{@FA{graph}.getVertices()}
///
/// Returns an iterator for all vertices of the graph. The iterator supports the
/// methods @FN{hasNext} and @FN{next}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-vertices
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertices = function () {
  var that = this,
    all = this._vertices.all(),
    v,
    Iterator;

  Iterator = function () {
    this.next = function next() {
      v = all.next();

      if (v === undefined) {
        return undefined;
      }

      return that.constructVertex(v._id);
    };

    this.hasNext = function hasNext() {
      return all.hasNext();
    };

    this._PRINT = function (seen, path, names) {
      // Ignores the standard arguments
      seen = path = names = null;

      internal.output("[vertex iterator]");
    };
  };

  return new Iterator();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an edge given its id
///
/// @FUN{@FA{graph}.getEdge(@FA{id})}
///
/// Returns the edge identified by @FA{id} or @LIT{null}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdge = function (id) {
  var ref,
    edge;

  ref = this._edges.firstExample('$id', id);

  if (ref !== null) {
    edge = this.constructEdge(ref._id);
  } else {
    try {
      edge = this.constructEdge(id);
    } catch (e) {
      edge = null;
    }
  }

  return edge;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all edges
///
/// @FUN{@FA{graph}.getEdges()}
///
/// Returns an iterator for all edges of the graph. The iterator supports the
/// methods @FN{hasNext} and @FN{next}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdges = function () {
  var that = this,
    all = this._edges.all(),
    v,
    Iterator;

  Iterator = function () {
    this.next = function next() {
      v = all.next();

      if (v === undefined) {
        return undefined;
      }

      return that.constructEdge(v._id);
    };

    this.hasNext = function hasNext() {
      return all.hasNext();
    };

    this._PRINT = function (seen, path, names) {
      // Ignores the standard arguments
      seen = path = names = null;

      internal.output("[edge iterator]");
    };
  };

  return new Iterator();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a vertex and all in- or out-bound edges
///
/// @FUN{@FA{graph}.removeVertex(@FA{vertex})}
///
/// Deletes the @FA{vertex} and all its edges.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-remove-vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeVertex = function (vertex) {
  var result,
    graph = this;

  this.emptyCachedPredecessors();

  if (vertex._id) {
    result = this._vertices.remove(vertex._properties);

    if (!result) {
      throw "cannot delete vertex";
    }

    vertex.edges().forEach(function (edge) {
      graph.removeEdge(edge);
    });

    vertex._id = undefined;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an edge
///
/// @FUN{@FA{graph}.removeEdge(@FA{vertex})}
///
/// Deletes the @FA{edge}. Note that the in and out vertices are left untouched.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-remove-edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeEdge = function (edge) {
  var result;

  this.emptyCachedPredecessors();

  if (edge._id) {
    result = this._edges.remove(edge._properties);

    if (!result) {
      throw "cannot delete edge";
    }

    edge._id = undefined;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Empty the internal cache for Predecessors
///
/// @FUN{@FA{graph}.emptyCachedPredecessors()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.emptyCachedPredecessors = function () {
  this.predecessors = {};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get Predecessors for a pair from the internal cache
///
/// @FUN{@FA{graph}.getCachedPredecessors(@FA{target}), @FA{source})}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getCachedPredecessors = function (target, source) {
  var predecessors;

  if (this.predecessors[target.getId()]) {
    predecessors = this.predecessors[target.getId()][source.getId()];
  }

  return predecessors;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Set Predecessors for a pair in the internal cache
///
/// @FUN{@FA{graph}.setCachedPredecessors(@FA{target}), @FA{source}, @FA{value})}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.setCachedPredecessors = function (target, source, value) {
  if (!this.predecessors[target.getId()]) {
    this.predecessors[target.getId()] = {};
  }

  this.predecessors[target.getId()][source.getId()] = value;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief number of vertices
///
/// @FUN{@FA{graph}.order()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.order = function () {
  return this._vertices.count();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief number of edges
///
/// @FUN{@FA{graph}.size()}
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.size = function () {
  return this._edges.count();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return all shortest paths
///
/// @FUN{@FA{graph}.geodesics()}
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.geodesics = function (options) {
  var sources = this._vertices.toArray(),
    targets = sources.slice(),
    geodesics = [],
    graph = this,
    vertexConstructor;

  options = options || {};

  vertexConstructor = function (raw_vertex) {
    return graph.constructVertex(raw_vertex._id);
  };

  sources = sources.map(vertexConstructor);
  targets = targets.map(vertexConstructor);

  sources.forEach(function (source) {
    targets = targets.slice(1);

    targets.forEach(function (target) {
      var pathes = source.pathTo(target);

      if (pathes.length > 0 && (!options.threshold || pathes[0].length > 2)) {
        if (options.grouped) {
          geodesics.push(pathes);
        } else {
          geodesics = geodesics.concat(pathes);
        }
      }
    });
  });

  return geodesics;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate a measurement
///
/// @FUN{@FA{graph}.measurement(@FA{measurement})}
///
/// Calculates the diameter or radius of a graph
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.measurement = function (measurement) {
  var graph = this,
    vertices = graph._vertices.toArray(),
    start_value;

  switch (measurement) {
  case "diameter":
    start_value = 0;
    break;
  case "radius":
    start_value = Infinity;
    break;
  default:
    throw "Unknown Measurement '" + measurement + "'";
  }

  return vertices.reduce(function (calculated, vertex) {
    vertex = graph.getVertex(vertex._id);

    switch (measurement) {
    case "diameter":
      calculated = Math.max(calculated, vertex.measurement("eccentricity"));
      break;
    case "radius":
      calculated = Math.min(calculated, vertex.measurement("eccentricity"));
      break;
    }

    return calculated;
  }, start_value);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate a normalized measurement
///
/// @FUN{@FA{graph}.normalizedMeasurement(@FA{measurement})}
///
/// Calculates the normalized degree, closeness, betweenness or eccentricity
/// of all vertices in a graph
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.normalizedMeasurement = function (measurement) {
  var graph = this,
    vertices = graph._vertices.toArray(),
    vertex_map,
    max = 0;

  vertex_map = vertices.reduce(function (map, raw_vertex) {
    var vertex = graph.constructVertex(raw_vertex._id),
      measured;

    switch(measurement) {
      case "closeness":
        measured = 1 / vertex.measurement("closeness");
        break;
      case "betweenness":
        measured = vertex.measurement("betweenness");
        break;
      case "eccentricity":
        measured = 1 / vertex.measurement("eccentricity");
        break;
      default:
        throw "Unknown measurement";
    }

    if (measured > max) {
      max = measured;
    }

    map[vertex.getId()] = measured;

    return map;
  }, {});

  Object.keys(vertex_map).forEach(function(key) {
    vertex_map[key] = vertex_map[key] / max;
  });

  return vertex_map;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief private function to construct a vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructVertex = function (id) {
  var vertex = this._weakVertices[id];

  if (vertex === undefined) {
    this._weakVertices[id] = vertex = new Vertex(this, id);
  }

  return vertex;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief private function to construct an edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructEdge = function (id) {
  var edge = this._weakEdges[id];

  if (edge === undefined) {
    this._weakEdges[id] = edge = new Edge(this, id);
  }

  return edge;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief graph printing
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function (seen, path, names) {
  var output;

  // Ignores the standard arguments
  seen = path = names = null;

  output = "Graph(\"";
  output += this._properties.name;
  output += "\")";
  internal.output(output);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.ArangoCollection = ArangoCollection;
exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
