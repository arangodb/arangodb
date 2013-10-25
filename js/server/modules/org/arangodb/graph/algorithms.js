/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

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

var graph = require("org/arangodb/graph");

var Edge = graph.Edge;
var Graph = graph.Graph;
var Vertex = graph.Vertex;

// -----------------------------------------------------------------------------
// --SECTION--                                       module "org/arangodb/graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                            VERTEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

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

  if (typeof(target_vertex) !== 'object') {
    throw "<target_vertex> must be a vertex object";
  }

  neighbor_set_one = this.getNeighbors(options).map(id_only);
  neighbor_set_two = target_vertex.getNeighbors(options).map(id_only);

  common_neighbors = neighbor_set_one.intersect(neighbor_set_two);

  if ((options.listed !== undefined) && (options.listed === true)) {
    return_value = common_neighbors;
  }
  else if ((options.normalized !== undefined) && (options.normalized === true)) {
    all_neighbors = neighbor_set_one.unite(neighbor_set_two);
    return_value = (common_neighbors.length / all_neighbors.length);
  }
  else {
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
/// @brief find the shortest path to a certain vertex, return the ID
///
/// @FUN{@FA{vertex}.pathTo(@FA{target_vertex}, @FA{options})}
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.pathTo = function (target_vertex, options) {
  if (typeof(target_vertex) !== 'object') {
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
// --SECTION--                                                             Graph
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return all shortest paths
///
/// @FUN{@FA{graph}.geodesics(@FA{options})}
///
/// Return all shortest paths
/// An optional `options` JSON object can be specified to control the result.
/// `options` can have the following sub-attributes:
/// - `grouped`: if not specified or set to `false`, the result will be a flat
///   list. If set to `true`, the result will be a list containing list of 
///   paths, grouped for each combination of source and target. 
/// - `threshold`: if not specified, all paths will be returned. If `threshold`
///   is `true`, only paths with a minimum length of 3 will be returned
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
/// Calculates the diameter or radius of a graph.
/// `measurement` can either be: 
/// - `diameter`: to calculate the diameter
/// - `radius`: to calculate the radius
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
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
