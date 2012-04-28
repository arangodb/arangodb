/*jslint indent: 2,
         nomen: true,
         maxlen: 80,
         sloppy: true,
         plusplus: true */
/*global require, WeakDictionary, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions functions
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal"),
  db = internal.db,
  edges = internal.edges,
  AvocadoCollection = internal.AvocadoCollection,
  AvocadoEdgesCollection = internal.AvocadoEdgesCollection,
  shallowCopy,
  propertyKeys,
  findOrCreateCollectionByName,
  findOrCreateEdgeCollectionByName;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shallow copy properties
////////////////////////////////////////////////////////////////////////////////

shallowCopy = function (props) {
  var shallow,
    key;

  shallow = {};

  for (key in props) {
    if (props.hasOwnProperty(key) && key[0] !== '_' && key[0] !== '$') {
      shallow[key] = props[key];
    }
  }

  return shallow;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief property keys
////////////////////////////////////////////////////////////////////////////////

propertyKeys = function (props) {
  var keys,
    key;

  keys = [];

  for (key in props) {
    if (props.hasOwnProperty(key) && key[0] !== '_' && key[0] !== '$') {
      keys.push(key);
    }
  }

  return keys;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

findOrCreateCollectionByName = function (name) {
  var col = internal.db._collection(name);

  if (col === null) {
    col = internal.db._create(name);
  } 
  else if (!(col instanceof AvocadoCollection)) {
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
  var col = internal.edges._collection(name);

  if (col === null) {
    col = internal.edges._create(name);
  } 
  else if (!(col instanceof AvocadoEdgesCollection)) {
    throw "<" + name + "> must be a document collection";
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
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new edge object
////////////////////////////////////////////////////////////////////////////////

function Edge(graph, id) {
  var props;

  this._graph = graph;
  this._id = id;

  props = this._graph._edges.document(this._id);

  if (props) {
    // extract the custom identifier, label, edges
    this._properties = props;
  } 
  else {
    // deleted
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
/// @addtogroup AvocadoGraph
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
  return propertyKeys(this._properties);
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
  var shallow = shallowCopy(this._properties),
    id;

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
  var shallow = shallowCopy(this._properties);

  delete shallow.$id;
  delete shallow.$label;

  delete shallow._id;
  delete shallow._rev;
  delete shallow._from;
  delete shallow._to;

  return shallow;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
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
  } 
  else if (this._properties.$id !== undefined) {
    if (typeof this._properties.$id === "string") {
      internal.output("Edge(\"", this._properties.$id, "\")");
    } 
    else {
      internal.output("Edge(", this._properties.$id, ")");
    }
  } 
  else {
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
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new vertex object
////////////////////////////////////////////////////////////////////////////////

function Vertex(graph, id) {
  var props;

  this._graph = graph;
  this._id = id;

  props = this._graph._vertices.document(this._id);

  if (props) {
    // extract the custom identifier
    this._properties = props;
  } 
  else {
    // deleted
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
/// @addtogroup AvocadoGraph
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
///
/// @verbinclude graph-vertex-add-in-edge3
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
  var query,
    result,
    graph,
    i;

  graph = this._graph;
  query = graph._edges.edges(this._id);

  result = [];

  for (i = 0;  i < query.length;  ++i) {
    result.push(graph.constructEdge(query[i]._id));
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of a vertex
///
/// @FUN{@FA{vertex}.getId()}
///
/// Returns the identifier of the @FA{vertex}. If the vertex was deleted, then
/// @CODE{undefined} is returned.
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
  var labels,
    result,
    i;

  if (arguments.length === 0) {
    result = this.inbound();
  } 
  else {
    labels = {};

    for (i = 0;  i < arguments.length;  ++i) {
      labels[arguments[i]] = true;
    }

    edges = this.inbound();
    result = [];

    for (i = 0;  i < edges.length;  ++i) {
      if (labels.hasOwnProperty(edges[i].getLabel())) {
        result.push(edges[i]);
      }
    }
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
  var labels,
    result,
    i;

  if (arguments.length === 0) {
    result = this.outbound();
  } 
  else {
    labels = {};
    for (i = 0;  i < arguments.length;  ++i) {
      labels[arguments[i]] = true;
    }

    edges = this.outbound();
    result = [];

    for (i = 0;  i < edges.length;  ++i) {
      if (labels.hasOwnProperty(edges[i].getLabel())) {
        result.push(edges[i]);
      }
    }
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
  return propertyKeys(this._properties);
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
  var query,
    result,
    graph,
    i;

  graph = this._graph;
  query = graph._edges.inEdges(this._id);

  result = [];

  for (i = 0;  i < query.length;  ++i) {
    result.push(graph.constructEdge(query[i]._id));
  }

  return result;
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
  var query,
    result,
    graph,
    i;

  graph = this._graph;
  query = graph._edges.outEdges(this._id);

  result = [];

  for (i = 0;  i < query.length;  ++i) {
    result.push(graph.constructEdge(query[i]._id));
  }

  return result;
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
  var shallow = shallowCopy(this._properties);

  delete shallow.$id;

  delete shallow._id;
  delete shallow._rev;

  return shallow;
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
  var shallow = shallowCopy(this._properties),
    id;

  shallow.$id = this._properties.$id;
  shallow[name] = value;

  // TODO use "update" if this becomes available
  id = this._graph._vertices.replace(this._id, shallow);
  this._properties = this._graph._vertices.document(id);

  return value;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
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
  } 
  else if (this._properties.$id !== undefined) {
    if (typeof this._properties.$id === "string") {
      internal.output("Vertex(\"", this._properties.$id, "\")");
    } 
    else {
      internal.output("Vertex(", this._properties.$id, ")");
    }
  } 
  else {
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
/// @addtogroup AvocadoGraph
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
  var gdb,
    graphProperties,
    graphPropertiesId,
    optionsForGraphCreation;

  gdb = internal.db._collection("_graph");

  if (gdb === null) {
    optionsForGraphCreation = { waitForSync : true, isSystem : true };
    gdb = internal.db._create("_graph", optionsForGraphCreation);

    // Currently buggy:
    // gdb.ensureUniqueConstraint("name");
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
      }
      catch (e) {
        throw "no graph named '" + name + "' found";
      }
      
      if (graphProperties === null) {
        throw "no graph named '" + name + "' found";
      }
      throw "no graph named '" + name + "' found";
    }

    vertices = internal.db._collection(graphProperties.vertices);

    if (vertices === null) {
      throw "vertex collection '" + graphProperties.vertices + "' has vanished";
    }

    edges = internal.edges._collection(graphProperties.edges);

    if (edges === null) {
      throw "edge collection '" + graphProperties.edges + "' has vanished";
    }
  } 
  else if (typeof vertices !== "string" || vertices === "") {
    throw "<vertices> must be a string or null";
  } 
  else if (typeof edges !== "string" || edges === "") {
    throw "<edges> must be a string or null";
  } 
  else {

    // Create a new graph or get an existing graph
    vertices = findOrCreateCollectionByName(vertices);
    edges = findOrCreateEdgeCollectionByName(edges);

    // Currently buggy:
    // edges.ensureUniqueConstraint("$id");
    // vertices.ensureUniqueConstraint("$id");

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
      } 
      else {
        throw "found graph but has different <name>";
      }
    } 
    else {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
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
  var gdb;

  gdb = internal.db._collection("_graph");

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

  if (typeof label === 'object') {
    data = label;
    label = undefined;
  }

  if (label === undefined) {
    label = null;
  }

  if (data === null || typeof data !== "object") {
    data = {};
  }

  shallow = shallowCopy(data);

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
    data = {};
  }

  shallow = shallowCopy(data);

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
    }
    catch (e) {
      vertex = null;
    }
  } 

  return vertex;
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
  var that,
    all,
    v,
    Iterator;

  that = this;
  all = this._vertices.all();

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
  var that,
    all,
    v,
    Iterator;

  that = this;
  all = this._edges.all();

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
    i;

  if (!vertex._id) {
    return;
  }

  edges = vertex.edges();
  result = this._vertices.remove(vertex._properties);

  if (!result) {
    throw "cannot delete vertex";
  }

  vertex._id = undefined;

  for (i = 0;  i < edges.length;  ++i) {
    this.removeEdge(edges[i]);
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

  if (!edge._id) {
    return;
  }

  result = this._edges.remove(edge._properties);

  if (!result) {
    throw "cannot delete edge";
  }

  edge._id = undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
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
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.AvocadoCollection = AvocadoCollection;
exports.AvocadoEdgesCollection = AvocadoEdgesCollection;
exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
