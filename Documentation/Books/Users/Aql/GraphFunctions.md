Graph functions
===============

AQL has the following functions to traverse loosely-structured graphs consisting of 
an edge collection and plus the vertices connected by it. If you have created a graph 
using the general-graph module you may want to use its more specific [Graph operations](../Aql/GraphOperations.md) 
instead.


Determining direct connections
------------------------------

### Edges
*EDGES(edgecollection, startvertex, direction, edgeexamples, options)*:

Return all edges connected to the vertex *startvertex* as an array. The possible values for
*direction* are:
  - *outbound*: Return all outbound edges
  - *inbound*: Return all inbound edges
  - *any*: Return outbound and inbound edges
  
The *edgeexamples* parameter can optionally be used to restrict the results to specific
edge connections only. The matching is then done via the *MATCHES* function.
To not restrict the result to specific connections, *edgeexamples* should be left
unspecified. 

Options is a JSON object that can contain the following properties:
- *includeVertices*: A boolean value, when set to true the return format will be modified.
     The function will then return an object with two attributes *edge* and *vertex* containing
     the respective elements.

In order to specify only options but no *edgeExamples* set *edgeExamples* to *null*.

*Examples*
 
      EDGES(friendrelations, "friends/john", "outbound")
      EDGES(friendrelations, "friends/john", "any", [ { "$label": "knows" } ])
      EDGES(friendrelations, "friends/john", "any", [ { "$label": "knows" } ], {includeVertices: true})
      EDGES(friendrelations, "friends/john", "any", null, {includeVertices: true})


### Neighbors
*NEIGHBORS(vertexcollection, edgecollection, startvertex, direction, edgeexamples, options)*:

Returns `_id` values of all distinct neighbors that are directly connected to the
vertex *startvertex* as an array.

The possible values for *direction* are:
- *outbound*: Return all outbound edges
- *inbound*: Return all inbound edges
- *any*: Return outbound and inbound edges

The *edgeexamples* parameter can optionally be used to restrict the results to specific
edge connections only. The matching is then done via the *MATCHES* function.
To not restrict the result to specific connections, *edgeexamples* should be left
unspecified.

The *options* parameter is an object and used to modify the result.
Available options:

* *includeData* is a boolean value to define if the returned documents should be extracted instead
  of returning their ids only. The default is *false*.

Note: in ArangoDB versions prior to 2.6 *NEIGHBORS* returned the array of neighbor vertices together
with their connecting edges including all attributes and not only the ids. 
To include the data and not only the ids set the *includeData* option to *true* in 2.6 and above.
To create the result format of 2.5 and earlier please replace the NEIGHBORS function by EDGES
with the option *includeVertices* set to true.

*Examples*

      NEIGHBORS(friends, friendrelations, "friends/john", "outbound")
      NEIGHBORS(users, usersrelations, "users/john", "any", [ { "$label": "recommends" } ] )
      NEIGHBORS(users, usersrelations, "users/john", "any", [ { "$label": "recommends" } ], {includeData: true})


General Purpose Traversals
--------------------------

General purpose traversals with its extendability by visitor functions offer more possibilities over the newer [AQL graph traversals](../Aql/GraphTraversals.md),
however unless you need some of these features you should [prefer AQL graph traversals](../Aql/GraphTraversals.md).

### Traversal
*TRAVERSAL(vertexcollection, edgecollection, startVertex, direction, options)*: 
Traverses the graph described by *vertexcollection* and *edgecollection*, 
starting at the vertex identified by id *startVertex*. Vertex connectivity is
specified by the *direction* parameter:
- *"outbound"*: Vertices are connected in *_from* to *_to* order
- *"inbound"*: Vertices are connected in *_to* to *_from* order
- *"any"*: Vertices are connected in both *_to* to *_from* and in 
  *_from* to *_to* order

Additional options for the traversal can be provided via the *options* document:
- *strategy*: Defines the traversal strategy. Possible values are *depthfirst* 
  and *breadthfirst*. Defaults to *depthfirst*
- *order*: Defines the traversal order: Possible values are *preorder*, *postorder*,
  and *preorder-expander*. Defaults to *preorder*
- *itemOrder*: Defines the level item order. Can be *forward* or 
  *backward*. Defaults to *forward*
- *minDepth*: Minimum path depths for vertices to be included. This can be used to
  include only vertices in the result that are found after a certain minimum depth.
  Defaults to 0 
- *maxIterations*: Maximum number of iterations in each traversal. This number can be
  set to prevent endless loops in traversal of cyclic graphs. When a traversal performs
  as many iterations as the *maxIterations* value, the traversal will abort with an
  error. If *maxIterations* is not set, a server-defined value will be used
- *maxDepth*: Maximum path depth for sub-edges expansion. This can be used to 
  limit the depth of the traversal to a sensible amount. This should especially be used
  for big graphs to limit the traversal to some sensible amount, and for graphs 
  containing cycles to prevent infinite traversals. The maximum depth defaults to 256, 
  with the chance of this value being non-sensical. For several graphs, a much lower
  maximum depth is sensible, whereas for other, more array-oriented graphs a higher
  depth should be used
- *paths*: If *true*, the paths encountered during the traversal will
  also be returned along with each traversed vertex. If *false*, only the 
  encountered vertices will be returned.
- *uniqueness*: An optional document with the following attributes:
  - *vertices*: 
    - *none*: No vertex uniqueness is enforced
    - *global*: A vertex may be visited at most once. This is the default.
    - *path*: A vertex is visited only if not already contained in the current
      traversal path
  - *edges*: 
    - *none*: No edge uniqueness is enforced
    - *global*: An edge may be visited at most once. This is the default
    - *path*: An edge is visited only if not already contained in the current
      traversal path
- *followEdges*: An optional array of example edge documents that the traversal will
  expand into. If no examples are given, the traversal will follow all edges. If one
  or many edge examples are given, the traversal will only follow an edge if it matches
  at least one of the specified examples. *followEdges* can also be a string with the
  name of an AQL user-defined function that should be responsible for checking if an
  edge should be followed. In this case, the AQL function will is expected to have the
  following signature:

      function (config, vertex, edge, path)

  The function is expected to return a boolean value. If it returns *true*, the edge
  will be followed. If *false* is returned, the edge will be ignored.

- *filterVertices*: An optional array of example vertex documents that the traversal will
  treat specially. If no examples are given, the traversal will handle all encountered
  vertices equally. If one or many vertex examples are given, the traversal will exclude
  any non-matching vertex from the result and/or not descend into it. Optionally,
  *filterVertices* can contain a string containing the name of a user-defined AQL function 
  that should be responsible for filtering. If so, the AQL function is expected to have the 
  following signature:

      function (config, vertex, path)

  If a custom AQL function is used for *filterVertices*, it is expected to return one of 
  the following values:
  - *[ ]*: Include the vertex in the result and descend into its connected edges
  - *[ "prune" ]*: Will include the vertex in the result but not descend into its connected edges
  - *[ "exclude" ]*: Will not include the vertex in the result but descend into its connected edges
  - *[ "prune", "exclude" ]*: Will completely ignore the vertex and its connected edges

- *vertexFilterMethod*: Only useful in conjunction with *filterVertices* and if no user-defined
  AQL function is used. If specified, it will influence how vertices are handled that don't match 
  the examples in *filterVertices*:
  - *[ "prune" ]*: Will include non-matching vertices in the result but not descend into them
  - *[ "exclude" ]*: Will not include non-matching vertices in the result but descend into them
  - *[ "prune", "exclude" ]*: Will completely ignore the vertex and its connected edges

- *visitor*: If specified, must be a string containing the name of a custom AQL function that
  will be called whenever the traversal visits a vertex. The custom AQL function is expected 
  to have the following signature:

      function (config, result, vertex, path, connections)

  There are two modes for custom AQL visitor functions, depending on the value of the option
  *visitorReturnsResults*. If it is set to *true*, then any value returned by the visitor will
  be appended to the list of results of the TRAVERSAL function. If *visitorReturnsResults* is
  set to *false*, then any value returned by the custom visitor function will be ignored.
  Instead, the visitor function is allowed to modify its *result* function parameter value 
  in-place. The contents of this variable at the end of the traversal will then be the
  results of the TRAVERSAL function.

  Note: The *connections* function parameter value will contain the edges connected to the 
  vertex only if *order* was set to *preorder-expander*. Otherwise, the value of this parameter 
  will be *undefined*.

  The following custom visitor functions are predefined and can be used by specifying the function
  name in the *visitor* attribute:

  - *"_AQL::HASATTRIBUTESVISITOR"*: this visitor will produce an object if it contains the
    attributes specified in *data.attributes* for each visited vertex. If *data.type* is *"all"*,
    then the object will only be returned if all specified attributes are present in the vertex,
    if *data.type* is *"any"*, the object will be returned if the vertex contains any of the
    specified attributes. If the optional *data.allowNull* is set to `false`, then the object
    will only be returned if an attribute is present **and** does not contain a `null` value.
    Otherwise an object will be returned already if the attribute is present.

  - *"_AQL::PROJECTINGVISITOR"*: this visitor will produce an object with the attributes 
    specified in *data.attributes* for each visited vertex. This can be used to create a 
    projection of each visited vertex' document.

  - *"_AQL::IDVISITOR"*: this visitor will return the _id attribute of each visited vertex.

  - *"_AQL::KEYVISITOR"*: this visitor will return the _key attribute of each visited vertex.

  - *"_AQL::COUNTINGVISITOR"*: this visitor will return a single number indicating the number
    of vertices visited.


- *visitorReturnsResults*: only useful in combination with a custom AQL visitor function. If
  set to *true*, the data returned by the visitor will be appended to the result. If set to
  *false*, any return value of the visitor function will be ignored. Instead, the visitor 
  function can modify its *result* parameter value in-place. At the end of the traversal,
  *result* is expected to be an array.

- *data*: only useful in combination with custom AQL visitor or filter functions. This attribute can
  be used to pass arbitrary data into the custom visitor function. The value contained in the
  *data* attribute will be made available to the *visitor* and *filterVertices* functions in the *config.data*
  attribute.

By default, the result of the TRAVERSAL function is an array of traversed points. Each point 
is an object consisting of the following attributes:

- *vertex*: The vertex at the traversal point
- *path*: The path history for the traversal point. The path is a document with the
  attributes *vertices* and *edges*, which are both arrays. Note that *path* is only present
  in the result if the *paths* attribute is set in the *options*

When using a custom AQL function as a visitor, the result may have a different structure.


*Examples*

      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        strategy: "depthfirst",
        order: "postorder",
        itemOrder: "backward",
        maxDepth: 6,
        paths: true
      })

      // filtering on specific edges (by specifying example edges)
      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        strategy: "breadthfirst",
        order: "preorder",
        itemOrder: "forward",
        followEdges: [ { type: "knows" }, { state: "FL" } ]
      })

      // filtering on specific edges and vertices
      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        strategy: "breadthfirst",
        order: "preorder",
        itemOrder: "forward",
        followEdges: [ { type: "knows" }, { state: "FL" } ],
        filterVertices: [ { isActive: true }, { isDeleted: false } ],
        vertexFilterMethod: [ "prune", "exclude" ]
      })

      // using user-defined AQL functions for edge and vertex filtering
      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        followEdges: "myfunctions::checkedge",
        filterVertices: "myfunctions::checkvertex"
      })

      // to register the custom AQL functions, execute something in the fashion of the 
      // following commands in arangosh once: 
      var aqlfunctions = require("org/arangodb/aql/functions");

      // these are the actual filter functions
      aqlfunctions.register("myfunctions::checkedge", function (config, vertex, edge, path) { 
        return (edge.type !== 'dislikes'); // don't follow these edges
      }, false);

      aqlfunctions.register("myfunctions::checkvertex", function (config, vertex, path) { 
        if (vertex.isDeleted || ! vertex.isActive) {
          return [ "prune", "exclude" ]; // exclude these and don't follow them
        }
        return [ ]; // include everything else
      }, false);


      // using a custom visitor to return only leaf-nodes
      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        visitor: "myfunctions::leafnodevisitor",
        visitorReturnsResults: true,
        order: "preorder-expander"
      })

      // to register the custom AQL function visitor that only returns leaf nodes, run
      // the following commands in arangosh once:
      var aqlfunctions = require("org/arangodb/aql/functions");

      aqlfunctions.register("myfunctions::leafnodevisitor", function (config, vertex, edge, path, connections) { 
        if (connected && connected.length === 0) {
          // will put the vertex into the result if it does not have any further connections
          return vertex;
        }
      }, false);


      // using a custom visitor to only count leaf-nodes
      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        visitor: "myfunctions::countingleafnodevisitor",
        visitorReturnsResults: false,
        order: "preorder-expander"
      })

      // registering the custom counting leaf node visitor function
      aqlfunctions.register("myfunctions::countingleafnodevisitor", function (config, vertex, edge, path, connections) { 
        if (result.length === 0) {
          // first call, push 0 into the result
          result.push(0);
        }

        if (connected && connected.length === 0) {
          // found a leaf node. now increase our counter
          result[0]++;
        }
      }, false);


      // using one of the predefined visitor functions
      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        visitor: "_AQL::COUNTINGVISITOR"
      })


      // passing data into a visitor
      TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
        visitor: "_AQL::PROJECTINGVISITOR",
        data: { attributes: [ "_id", "_key", "name" ] }
      })

### Traversal Tree
*TRAVERSAL_TREE(vertexcollection, edgecollection, startVertex, direction, connectName, options)*: 
Traverses the graph described by *vertexcollection* and *edgecollection*, 
starting at the vertex identified by id *startVertex* and creates a hierarchical result.
Vertex connectivity is establish by inserted an attribute which has the name specified via
the *connectName* parameter. Connected vertices will be placed in this attribute as an 
array.

The *options* are the same as for the *TRAVERSAL* function, except that the result will
be set up in a way that resembles a depth-first, pre-order visitation result. Thus, the
*strategy* and *order* attributes of the *options* attribute will be ignored.

*Examples*

      TRAVERSAL_TREE(friends, friendrelations, "friends/john", "outbound", "likes", { 
        itemOrder: "forward"
      })

### Shortest Path
*SHORTEST_PATH(vertexcollection, edgecollection, startVertex, endVertex, direction, options)*: 
Determines the first shortest path from the *startVertex* to the *endVertex*.
Both vertices must be present in the vertex collection specified in *vertexcollection*,
and any connecting edges must be present in the collection specified by *edgecollection*.
Vertex connectivity is specified by the *direction* parameter:
- *"outbound"*: Vertices are connected in *_from* to *_to* order
- *"inbound"*: Vertices are connected in *_to* to *_from* order
- *"any"*: Vertices are connected in both *_to* to *_from* and in 
  *_from* to *_to* order
The search is aborted when a shortest path is found. Only the first shortest path will be
returned. Any vertex will be visited at most once by the search.

Additional options for the shortest path can be provided via the *options* document:
- *maxIterations*: Maximum number of iterations in the search. This number can be
  set to bound long-running searches. When a search performs as many iterations as the 
  *maxIterations* value, the search will abort with an error. If *maxIterations* is not 
  set, a server-defined value may be used.
- *includeData*: Defines if the documents found on the path should be extracted or not.
     Default *false*, will only return the *\_id* values.
- *distance*: a custom function with the following signature:

      function (config, vertex1, vertex2, edge)

  Both vertices and the connecting edge will be passed into the function. The function
  is expected to return a numeric value that expresses the distance between the two
  vertices. Higher values will mean higher distances, giving the connection a lower
  priority in further analysis.
  If this options is not specified all vertices are assumed to have the
  same distance (1) to each other. If a function name is specified, it must have been
  registered as a regular user-defined AQL function.

- *weight*: The name of a attribute weight attribute.
    If so this specific attribute will be examined on every edge on the path and will
    be used as a distance. In order to let this work properly you have to make sure
    to store only numerical values in this attribute. If the attribute is not present
    on any edge the value given in the *defaultWeight* option is used as a replacement.
    Using a weight attribute is only allowed in combination with *defaultWeight*.

- *defaultWeight*: Only useful in combination with an attribute name given in *weight*.
    If any edge does not have the attribute required for *weight* the numeric value
    defined by this option is used.

- *followEdges*: An optional array of example edge documents that the search will
  expand into. If no examples are given, the search will follow all edges. If one
  or many edge examples are given, the search will only follow an edge if it matches
  at least one of the specified examples. *followEdges* can also be a string with the
  name of an AQL user-defined function that should be responsible for checking if an
  edge should be followed. In this case, the AQL function will is expected to have the
  following signature:

      function (config, vertex, edge, path)

  The function is expected to return a boolean value. If it returns *true*, the edge
  will be followed. If *false* is returned, the edge will be ignored.

- *filterVertices*: An optional array of example vertex documents that the search will
  treat specially. If no examples are given, the search will handle all encountered
  vertices equally. If one or many vertex examples are given, the search will exclude
  the vertex from the result and/or not descend into it. Optionally, *filterVertices* can 
  be a string containing the name of a user-defined AQL function that should be responsible 
  for filtering. If so, the custom AQL function is expected to have the following signature:

      function (config, vertex, path)

  If a custom AQL function is used, it is expected to return one of the following values:

  - *[ ]*: Include the vertex in the result and descend into its connected edges
  - *[ "prune" ]*: Will include the vertex in the result but not descend into its connected edges
  - *[ "exclude" ]*: Will not include the vertex in the result but descend into its connected edges
  - *[ "prune", "exclude" ]*: Will completely ignore the vertex and its connected edges

- *visitor*: If specified, must be a string containing the name of a custom AQL function that
  will be called whenever the traversal visits a vertex. The custom AQL function is expected 
  to have the following signature:

      function (config, result, vertex, path, connections)

  There are two modes for custom AQL visitor functions, depending on the value of the option
  *visitorReturnsResults*. If it is set to *true*, then any value returned by the visitor will
  be appended to the list of results of the TRAVERSAL function. If *visitorReturnsResults* is
  set to *false*, then any value returned by the custom visitor function will be ignored.
  Instead, the visitor function is allowed to modify its *result* function parameter value 
  in-place. The contents of this variable at the end of the traversal will then be the
  results of the TRAVERSAL function.

  Note: The *connections* function parameter value will contain the edges connected to the 
  vertex only if *order* was set to *preorder-expander*. Otherwise, the value of this parameter 
  will be *undefined*.

- *visitorReturnsResults*: only useful in combination with a custom AQL visitor function. If
  set to *true*, the data returned by the visitor will be appended to the result. If set to
  *false*, any return value of the visitor function will be ignored. Instead, the visitor 
  function can modify its *result* parameter value in-place. At the end of the traversal,
  *result* is expected to be an array.

- *data*: only useful in combination with custom AQL visitor or filter functions. This attribute can
  be used to pass arbitrary data into the custom visitor function. The value contained in the
  *data* attribute will be made available to the *visitor* and *filterVertices* functions in the *config.data*
  attribute.

By default, the result of the SHORTEST_PATH function is an object consisting of the following attributes:
- *vertices*: The list of all vertices encountered on the path.
- *edges*: The list of all edges encountered on the path.
- *distance*: The total distance needed on this path. Using the given distance computation.

When using a custom AQL function as a visitor, the result may have a different structure.
 

*Examples*

      SHORTEST_PATH(cities, motorways, "cities/CGN", "cities/MUC", "outbound")

      // using a user-defined distance function
      SHORTEST_PATH(cities, motorways, "cities/CGN", "cities/MUC", "outbound", {
        paths: true,
        distance: "myfunctions::citydistance"
      })

      // using a user-defined function to filter edges
      SHORTEST_PATH(cities, motorways, "cities/CGN", "cities/MUC", "outbound", {
        paths: true,
        followEdges: "myfunctions::checkedge"
      })

      // to register a custom AQL distance function, execute something in the fashion of the 
      // following commands in arangosh once: 
      var aqlfunctions = require("org/arangodb/aql/functions");

      // this is the actual distance function
      aqlfunctions.register("myfunctions::distance", function (config, vertex1, vertex2, edge) { 
        return Math.sqrt(Math.pow(vertex1.x - vertex2.x) + Math.pow(vertex1.y - vertex2.y));
      }, false);

      // this is the filter function for the edges
      aqlfunctions.register("myfunctions::checkedge", function (config, vertex, edge, path) { 
        return (edge.underConstruction === false); // don't follow these edges
      }, false);

Other functions
---------------

### Paths
*PATHS(vertexcollection, edgecollection, direction, options)*: 
returns an array of paths through the graph defined by the nodes in the collection 
*vertexcollection* and edges in the collection *edgecollection*. For each vertex
in *vertexcollection*, it will determine the paths through the graph depending on the
value of *direction*:
- *"outbound"*: Follow all paths that start at the current vertex and lead to another vertex
- *"inbound"*: Follow all paths that lead from another vertex to the current vertex
- *"any"*: Combination of *"outbound"* and *"inbound"*
The default value for *direction* is *"outbound"*.
If specified, *options* must be a JavaScript object with the following optional attributes:
- *minLength*: specifies the minimum length of the paths to be returned. The default is 0.
- *maxLength*: specifies the maximum length of the paths to be returned. The default is 10.
- *followcycles*: if true, cyclic paths will be followed as well. This is turned off by
  default.

The result of the function is an array of paths. Paths of length 0 will also be returned. Each 
path is a document consisting of the following attributes:
- *vertices*: array of vertices visited along the path
- *edges*: array of edges visited along the path (may be empty)
- *source*: start vertex of path
- *destination*: destination vertex of path

*Examples*

      PATHS(friends, friendrelations, "outbound")

      FOR p IN PATHS(friends, friendrelations, "outbound", { minLength: 2, maxLength: 2 }) 
        FILTER p.source._id == "users/123456"
        RETURN p.vertices[*].name

Graph consistency
-----------------
When [using the graph management functions to remove vertices](../GeneralGraphs/Management.md#remove-a-vertex)
you have the guaranty that all referencing edges are also removed.
However, if you use document features alone to remove vertices, no edge collections will be adjusted.
This results in an edge with its  `_from` or `_to` attribute referring to vanished vertices.

Now we query such a graph using the [neighbors](#neighbors) or the [shortest path](#shortest-path) functions.
If *includeData* wasn't enabled, the referred missing vertex will not be queried, and thus the result
set will contain the referral to this missing vertex.
If *includeData* is enabled, these missing vertices will be touched by the query.

In order to keep the result set consistent between *includeData* enabled or disabled, a `null` will be padded to fill the gap for each missing vertex.

Performance notes
-----------------

When using any of AQL's general purpose traversal functions, please make sure that the graph 
does not contain cycles, or that you at least specify some maximum depth or uniqueness 
criteria for a traversal. In contrast [AQL graph traversals](../Aql/GraphTraversals.md) won't trap on cycles.

If no bounds are set, a traversal may run into an endless loop in a cyclic graph or sub-graph.
Even in a non-cyclic graph, traversing far into the graph may consume a lot of processing
time and memory to create the result set.

By default, traversals are restricted to process a limited amount of vertices only and then
abort. This is to protect the user from unintentionally running a traversal that will not 
abort or consume lots of resources. The maximum number of vertices that a traversal will
process is specified by the optional *maxIterations* configuration value. If the number of
vertices processed in a traversal reaches this cap will, the traversal will abort and throw
a *too many iterations* exception.

