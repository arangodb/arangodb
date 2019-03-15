Using Traversal Objects
=======================

{% hint 'warning' %}
The JavaScript module `@arangodb/graph/traversal` (*traversal module* for short)
is deprecated from version 3.4.0 on. The preferred way to traverse graphs is with AQL.
{% endhint %}

To use a traversal object, we first need to require the *traversal* module:


```js
var traversal = require("@arangodb/graph/traversal");
var examples = require("@arangodb/graph-examples/example-graph.js");
examples.loadGraph("worldCountry");
```

We then need to setup a configuration for the traversal and determine at which vertex to 
start the traversal:

```js
var config = {
  datasource: traversal.generalGraphDatasourceFactory("worldCountry"),
  strategy: "depthfirst",
  order: "preorder",
  filter: traversal.visitAllFilter,
  expander: traversal.inboundExpander,
  maxDepth: 1
};

var startVertex = db._document("v/world");
```

**Note**: The startVertex needs to be a document, not only a document id.

We can then create a traverser and start the traversal by calling its *traverse* method.
Note that *traverse* needs a *result* object, which it can modify in place:

```js
var result = {
  visited: {
    vertices: [ ],
    paths: [ ]
  }
};
var traverser = new traversal.Traverser(config);
traverser.traverse(result, startVertex);
```

Finally, we can print the contents of the *results* object, limited to the visited vertices.
We will only print the name and type of each visited vertex for brevity:

```js
require("@arangodb").print(result.visited.vertices.map(function(vertex) {
  return vertex.name + " (" + vertex.type + ")";
}));
```


The full script, which includes all steps carried out so far is thus:

```js
var traversal = require("@arangodb/graph/traversal");

var config = {
  datasource: traversal.generalGraphDatasourceFactory("worldCountry"),
  strategy: "depthfirst",
  order: "preorder",
  filter: traversal.visitAllFilter,
  expander: traversal.inboundExpander,
  maxDepth: 1
};

var startVertex = db._document("v/world");
var result = {
  visited: {
    vertices: [ ],
    paths: [ ]
  }
};

var traverser = new traversal.Traverser(config);
traverser.traverse(result, startVertex);

require("@arangodb").print(result.visited.vertices.map(function(vertex) {
  return vertex.name + " (" + vertex.type + ")";
}));
```

The result is an array of vertices that were visited during the traversal, starting at the
start vertex (i.e. *v/world* in our example):

```js
[
  "World (root)",
  "Africa (continent)",
  "Asia (continent)",
  "Australia (continent)",
  "Europe (continent)",
  "North America (continent)",
  "South America (continent)"
]
```

**Note**: The result is limited to vertices directly connected to the start vertex. We
achieved this by setting the *maxDepth* attribute to *1*. Not setting it would return the
full array of vertices.

Traversal Direction
-------------------

For the examples contained in this manual, we'll be starting the traversals at vertex
*v/world*. Vertices in our graph are connected like this:

```js
v/world <- is-in <- continent (Africa) <- is-in <- country (Algeria) <- is-in <- capital (Algiers)
```

To get any meaningful results, we must traverse the graph in **inbound** order. This means,
we'll be following all incoming edges of to a vertex. In the traversal configuration, we
have specified this via the *expander* attribute:

```js
var config = {
  ...
  expander: traversal.inboundExpander
};
```

For other graphs, we might want to traverse via the **outgoing** edges. For this, we can
use the *outboundExpander*. There is also an *anyExpander*, which will follow both outgoing
and incoming edges. This should be used with care and the traversal should always be
limited to a maximum number of iterations (e.g. using the *maxIterations* attribute) in
order to terminate at some point.

To invoke the default outbound expander for a graph, simply use the predefined function:

```js
var config = {
  ...
  expander: traversal.outboundExpander
};
```

Please note the outbound expander will not produce any output for the examples if we still
start the traversal at the *v/world* vertex.

Still, we can use the outbound expander if we start somewhere else in the graph, e.g. 

```js
var traversal = require("@arangodb/graph/traversal");

var config = {
  datasource: traversal.generalGraphDatasourceFactory("world_graph"),
  strategy: "depthfirst",
  order: "preorder",
  filter: traversal.visitAllFilter,
  expander: traversal.outboundExpander
};

var startVertex = db._document("v/capital-algiers");
var result = {
  visited: {
    vertices: [ ],
    paths: [ ]
  }
};

var traverser = new traversal.Traverser(config);
traverser.traverse(result, startVertex);

require("@arangodb").print(result.visited.vertices.map(function(vertex) {
  return vertex.name + " (" + vertex.type + ")";
}));
```

The result is:

```js
[
  "Algiers (capital)",
  "Algeria (country)",
  "Africa (continent)",
  "World (root)"
]
```

which confirms that now we're going outbound.

Traversal Strategy
------------------

### Depth-first traversals

The visitation order of vertices is determined by the *strategy* and *order* attributes set
in the configuration. We chose *depthfirst* and *preorder*, meaning the traverser will 
visit each vertex **before** handling connected edges (pre-order), and descend into any 
connected edges before processing other vertices on the same level (depth-first). 

Let's remove the *maxDepth* attribute now. We'll now be getting all vertices (directly
and indirectly connected to the start vertex):

```js
var config = {
  datasource: traversal.generalGraphDatasourceFactory("world_graph"),
  strategy: "depthfirst",
  order: "preorder",
  filter: traversal.visitAllFilter,
  expander: traversal.inboundExpander
};

var result = {
  visited: {
    vertices: [ ],
    paths: [ ]
  }
};

var traverser = new traversal.Traverser(config);
traverser.traverse(result, startVertex);

require("@arangodb").print(result.visited.vertices.map(function(vertex) {
  return vertex.name + " (" + vertex.type + ")";
}));
```

The result will be a longer array, assembled in depth-first, pre-order order. For 
each continent found, the traverser will descend into linked countries, and then into
the linked capital:

```js
[
  "World (root)",
  "Africa (continent)",
  "Algeria (country)",
  "Algiers (capital)",
  "Angola (country)",
  "Luanda (capital)",
  "Botswana (country)",
  "Gaborone (capital)",
  "Burkina Faso (country)",
  "Ouagadougou (capital)",
  ...
]
```

Let's switch the *order* attribute from *preorder* to *postorder*. This will make the
traverser visit vertices **after** all connected vertices were visited (i.e. most distant
vertices will be emitted first):

```js
[
  "Algiers (capital)",
  "Algeria (country)",
  "Luanda (capital)",
  "Angola (country)",
  "Gaborone (capital)",
  "Botswana (country)",
  "Ouagadougou (capital)",
  "Burkina Faso (country)",
  "Bujumbura (capital)",
  "Burundi (country)",
  "Yaounde (capital)",
  "Cameroon (country)",
  "N'Djamena (capital)",
  "Chad (country)",
  "Yamoussoukro (capital)",
  "Cote d'Ivoire (country)",
  "Cairo (capital)",
  "Egypt (country)",
  "Asmara (capital)",
  "Eritrea (country)",
  "Africa (continent)",
  ...
]
```

### Breadth-first traversals

If we go back to *preorder*, but change the strategy to *breadth-first* and re-run the 
traversal, we'll see that the return order changes, and items on the same level will be 
returned adjacently:

```js
[
  "World (root)",
  "Africa (continent)",
  "Asia (continent)",
  "Australia (continent)",
  "Europe (continent)",
  "North America (continent)",
  "South America (continent)",
  "Burkina Faso (country)",
  "Burundi (country)",
  "Cameroon (country)",
  "Chad (country)",
  "Algeria (country)",
  "Angola (country)",
  ...
]
```

**Note**: The order of items returned for the same level is undefined.
This is because there is no natural order of edges for a vertex with
multiple connected edges. To explicitly set the order for edges on the
same level, you can specify an edge comparator function with the *sort*
attribute:

```js
var config = {
  ...
  sort: function (l, r) { return l._key < r._key ? 1 : -1; }
  ...
};
```

The arguments l and r are edge documents.
This will traverse edges of the same vertex in backward *_key* order:

```js
[
  "World (root)",
  "South America (continent)",
  "North America (continent)",
  "Europe (continent)",
  "Australia (continent)",
  "Asia (continent)",
  "Africa (continent)",
  "Ecuador (country)",
  "Colombia (country)",
  "Chile (country)",
  "Brazil (country)",
  "Bolivia (country)",
  "Argentina (country)",
  ...
]
```

**Note**: This attribute only works for the usual expanders
*traversal.inboundExpander*, *traversal.outboundExpander*,
*traversal.anyExpander* and their corresponding "WithLabels" variants.
If you are using custom expanders
you have to organize the sorting within the specified expander.

### Writing Custom Visitors

So far we have used much of the traverser's default functions. The traverser is very
configurable and many of the default functions can be overridden with custom functionality.

For example, we have been using the default visitor function (which is always used if
the configuration does not contain the *visitor* attribute). The default visitor function
is called for each vertex in a traversal, and will push it into the result.
This is the reason why the *result* variable looked different after the traversal, and
needed to be initialized before the traversal was started.

Note that the default visitor (named `trackingVisitor`) will add every visited vertex
into the result, including the full paths from the start vertex. This is useful for learning and
debugging purposes, but should be avoided in production because it might produce (and
copy) huge amounts of data. Instead, only those data should be copied into the result 
that are actually necessary.

The traverser comes with the following predefined visitors:
- *trackingVisitor*: this is the default visitor. It will copy all data of each visited
  vertex plus the full path information into the result. This can be slow if the result
  set is huge or vertices contain a lot of data.
- *countingVisitor*: this is a very lightweight visitor: all it does is increase a
  counter in the result for each vertex visited. Vertex data and paths will not be copied
  into the result.
- *doNothingVisitor*: if no action shall be carried out when a vertex is visited, this
  visitor can be employed. It will not do anything and will thus be fast. It can be used
  for performance comparisons with other visitors.

We can also write our own visitor function if we want to. The general function signature for
visitor functions is as follows:

```js
var config = {
  ...
  visitor: function (config, result, vertex, path, connected) { ... }
};
```

Note: the *connected* parameter value will only be set if the traversal order is
set to *preorder-expander*. Otherwise, this parameter won't be set by the traverser.

Visitor functions are not expected to return any values. Instead, they can modify the
*result* variable (e.g. by pushing the current vertex into it), or do anything else.
For example, we can create a simple visitor function that only prints information about
the current vertex as we traverse:

```js
var config = {
  datasource: traversal.generalGraphDatasourceFactory("world_graph"),
  strategy: "depthfirst",
  order: "preorder",
  filter: traversal.visitAllFilter,
  expander: traversal.inboundExpander,
  visitor: function (config, result, vertex, path) {
    require("@arangodb").print("visiting vertex", vertex.name);
  }
};

var traverser = new traversal.Traverser(config);
traverser.traverse(undefined, startVertex);
```

To write a visitor that increments a counter each time a vertex is visited,
we could write the following custom visitor:

```js
config.visitor = function (config, result, vertex, path, connected) {
  if (! result) {
    result = { };
  }

  if (! result.hasOwnProperty('count')) {
    result.count = 0;
  }

  ++result.count;
}
```

Note that such visitor is already predefined (it's the countingVisitor described
above). It can be used as follows:
```js
config.visitor = traversal.countingVisitor;
```

Another example of a visitor is one that collects the `_id` values of all vertices 
visited:

```js
config.visitor = function (config, result, vertex, path, connected) {
  if (! result) {
    result = { };
  }
  if (! result.hasOwnProperty("visited")) {
    result.visited = { vertices: [ ] };
  }

  result.visited.vertices.push(vertex._id);
}
```

When the traversal order is set to *preorder-expander*, the traverser will pass
a fifth parameter value into the visitor function. This parameter contains the
connected edges of the visited vertex as an array. This can be handy because in this
case the visitor will get all information about the vertex and the connected edges
together.

For example, the following visitor can be used to print only leaf nodes (that
do not have any further connected edges):

```js
config.visitor = function (config, result, vertex, path, connected) {
  if (connected && connected.length === 0) {
    require("@arangodb").print("found a leaf-node: ", vertex);
  }
}
```

Note that for this visitor to work, the traversal *order* attribute needs to be
set to the value *preorder-expander*.

Filtering Vertices and Edges
----------------------------

### Filtering Vertices

So far we have printed or returned all vertices that were visited during the traversal. 
This is not always required. If the result shall be restrict to just specific vertices, 
we can use a filter function for vertices. It can be defined by setting the *filter* 
attribute of a traversal configuration, e.g.:

```js
var config = {
  filter: function (config, vertex, path) {
    if (vertex.type !== 'capital') {
      return 'exclude';
    }
  }
}
```

The above filter function will exclude all vertices that do not have a *type* value of
*capital*. The filter function will be called for each vertex found during the traversal.
It will receive the traversal configuration, the current vertex, and the full path from 
the traversal start vertex to the current vertex. The path consists of an array of edges, 
and an array of vertices. We could also filter everything but capitals by checking the
length of the path from the start vertex to the current vertex. Capitals will have a
distance of 3 from the *v/world* start vertex
(capital → is-in → country → is-in → continent → is-in → world):

```js
var config = {
  ...
  filter: function (config, vertex, path) {
    if (path.edges.length < 3) {
      return 'exclude';
    }
  }
}
```

**Note**: If a filter function returns nothing (or *undefined*), the current vertex
will be included, and all connected edges will be followed. If a filter function
returns *exclude* the current vertex will be excluded from the result, and all still
all connected edges will be followed. If a filter function returns *prune*, the
current vertex will be included, but no connected edges will be followed.

For example, the following filter function will not descend into connected edges of
continents, limiting the depth of the traversal. Still, continent vertices will be
included in the result:

```js
var config = {
  ...
  filter: function (config, vertex, path) {
    if (vertex.type === 'continent') {
      return 'prune';
    }
  }
}
```

It is also possible to combine *exclude* and *prune* by returning an array with both
values:

```js
return [ 'exclude', 'prune' ];
```

Filtering Edges
---------------

It is possible to exclude certain edges from the traversal. To filter on edges, a
filter function can be defined via the *expandFilter* attribute. The *expandFilter*
is a function which is called for each edge during a traversal.

It will receive the current edge (*edge* variable) and the vertex which the edge
connects to (in the direction of the traversal). It also receives the current path
from the start vertex up to the current vertex (excluding the current edge and the
vertex the edge points to).

If the function returns *true*, the edge will be followed. If the function returns
*false*, the edge will not be followed.
Here is a very simple custom edge filter function implementation, which simply
includes edges if the (edges) path length is less than 1, and will exclude any
other edges. This will effectively terminate the traversal after the first level 
of edges:

```js
var config = {
  ...
  expandFilter: function (config, vertex, edge, path) {
    return (path.edges.length < 1);
  }
};
```

Writing Custom Expanders
------------------------

The edges connected to a vertex are determined by the expander. So far we have used a 
default expander (the default inbound expander to be precise). The default inbound 
expander simply enumerates all connected ingoing edges for a vertex, based on the
[edge collection](../../Appendix/Glossary.md#edge-collection) specified in the traversal configuration.

There is also a default outbound expander, which will enumerate all connected outgoing
edges. Finally, there is an any expander, which will follow both ingoing and outgoing
edges.

If connected edges must be determined in some different fashion for whatever reason, a
custom expander can be written and registered by setting the *expander* attribute of the 
configuration. The expander function signature is as follows:

```js
var config = {
  ...
  expander: function (config, vertex, path) { ... }
}
```

It is the expander's responsibility to return all edges and vertices directly
connected to the current vertex (which is passed via the *vertex* variable).
The full path from the start vertex up to the current vertex is also supplied via
the *path* variable.
An expander is expected to return an array of objects, which need to have an *edge*
and a *vertex* attribute each.

**Note**: If you want to rely on a particular order in which the edges
are traversed, you have to sort the edges returned by your expander
within the code of the expander. The functions to get outbound, inbound
or any edges from a vertex do not guarantee any particular order!

A custom implementation of an inbound expander could look like this (this is a
non-deterministic expander, which randomly decides whether or not to include
connected edges):

```js
var config = {
  ...
  expander: function (config, vertex, path) {
    var connected = [ ];
    var datasource = config.datasource;
    datasource.getInEdges(vertex._id).forEach(function (edge) {
      if (Math.random() >= 0.5) {
        connected.push({ edge: edge, vertex: (edge._from) });
      }
    });
    return connected;
  }
};
```

A custom expander can also be used as an edge filter because it has full control
over which edges will be returned.

Following are two examples of custom expanders that pick edges based on attributes
of the edges and the connected vertices.

Finding the connected edges / vertices based on an attribute *when* in the
connected vertices. The goal is to follow the edge that leads to the vertex
with the highest value in the *when* attribute:

```js
var config = {
  ...
  expander: function (config, vertex, path) {
    var datasource = config.datasource;
    // determine all outgoing edges
    var outEdges = datasource.getOutEdges(vertex);

    if (outEdges.length === 0) {
      return [ ];
    }

    var data = [ ];
    outEdges.forEach(function (edge) {
      data.push({ edge: edge, vertex: datasource.getInVertex(edge) });
    });

    // sort outgoing vertices according to "when" attribute value
    data.sort(function (l, r) {
      if (l.vertex.when === r.vertex.when) {
        return 0;
      }

      return (l.vertex.when < r.vertex.when ? 1 : -1);
    });

    // pick first vertex found (with highest "when" attribute value)
    return [ data[0] ];
  }
  ...
};
```

Finding the connected edges / vertices based on an attribute *when* in the
edge itself. The goal is to pick the one edge (out of potentially many) that
has the highest *when* attribute value:

```js
var config = {
  ...
  expander: function (config, vertex, path) {
    var datasource = config.datasource;
    // determine all outgoing edges
    var outEdges = datasource.getOutEdges(vertex);

    if (outEdges.length === 0) {
      return [ ]; // return an empty array
    }

    // sort all outgoing edges according to "when" attribute
    outEdges.sort(function (l, r) {
      if (l.when === r.when) {
        return 0;
      }
      return (l.when < r.when ? -1 : 1);
    });

    // return first edge (the one with highest "when" value)
    var edge = outEdges[0];
    try {
      var v = datasource.getInVertex(edge);
      return [ { edge: edge, vertex: v } ];
    }
    catch (e) { }

    return [ ];
  }
  ...
};
```


Handling Uniqueness
-------------------

Graphs may contain cycles. To be on top of what happens when a traversal encounters a vertex
or an edge it has already visited, there are configuration options.

The default configuration is to visit every vertex, regardless of whether it was already visited
in the same traversal. However, edges will by default only be followed if they are not already 
present in the current path.

Imagine the following graph which contains a cycle:

```
A -> B -> C -> A
```

When the traversal finds the edge from *C* to *A*, it will by default follow it. This is because
we have not seen this edge yet. It will also visit vertex *A* again. This is because by default
all vertices will be visited, regardless of whether already visited or not.

However, the traversal will not again following the outgoing edge from *A* to *B*. This is because
we already have the edge from *A* to *B* in our current path.

These default settings will prevent infinite traversals.

To adjust the uniqueness for visiting vertices, there are the following options for *uniqueness.vertices*:

* *"none"*: always visit a vertices, regardless of whether it was already visited or not
* *"global"*: visit a vertex only if it was not visited in the traversal
* *"path"*: visit a vertex if it is not included in the current path

To adjust the uniqueness for following edges, there are the following options for *uniqueness.edges*:

* *"none"*: always follow an edge, regardless of whether it was followed before
* *"global"*: follow an edge only if it wasn't followed in the traversal
* *"path"*: follow an edge if it is not included in the current path

Note that uniqueness checking will have some effect on both runtime and memory usage. For example, when
uniqueness checks are set to *"global"*, arrays of visited vertices and edges must be kept in memory while the
traversal is executed. Global uniqueness should thus only be used when a traversal is expected to visit
few nodes.

In terms of runtime, turning off uniqueness checks (by setting both options to *"none"*) is the best choice,
but it is only safe for graphs that do not contain cycles. When uniqueness checks are deactivated in a graph
with cycles, the traversal might not abort in a sensible amount of time.


Optimizations
-------------

There are a few options for making a traversal run faster.

The best option is to make the amount of visited vertices and followed edges as small as possible. This can
be achieved by writing custom filter and expander functions. Such functions should only include vertices of
interest, and only follow edges that might be interesting.

Traversal depth can also be bounded with the *minDepth* and *maxDepth* options.

Another way to speed up traversals is to write a custom visitor function. The default visitor function 
(*trackingVisitor*) will copy every visited vertex into the result. If vertices contain lots of data, 
this might be expensive. It is therefore recommended to only copy such data into the result that is actually 
needed. The default visitor function will also copy the full path to the visited document into the result. 
This is even more expensive and should be avoided if possible.

If the goal of a traversal is to only count the number of visited vertices, the prefab *countingVisitor*  
will be much more efficient than the default visitor.

For graphs that are known to not contain any cycles, uniqueness checks should be turned off. This can achieved
via the *uniqueness* configuration options. Note that uniqueness checks should not be turned off for graphs
that are known contain cycles or if there is no information about the graph's structure.

By default, a traversal will only process a limited number of vertices. This is protect the user from
unintentionally run a never-ending traversal on a graph with cyclic data. How many vertices will be processed
at most is determined by the *maxIterations* configuration option. If a traversal hits the cap specified 
by *maxIterations*, it will abort and throw a *too many iterations* exception. If this error is encountered,
the *maxIterations* value should be increased if it is made sure that the other traversal configuration
parameters are sane and the traversal will abort naturally at some point.

Finally, the *buildVertices* configuration option can be set to *false* to avoid looking up and fully constructing
vertex data. If all that's needed from vertices are the *_id* or *_key* attributes, the *buildvertices* 
option can be set to *false*. If visitor, filter or expandFilter functions need to access other vertex
attributes, the option should not be changed.


Configuration Overview
----------------------

This section summarizes the configuration attributes for the traversal object. The
configuration can consist of the following attributes:

- *visitor*: visitor function for vertices. It will be called for all non-excluded vertices. The
  general visitor function signature is *function (config, result, vertex, path)*. If the traversal
  order is *preorder-expander*, the connecting edges of the visited vertex will be passed as the
  fifth parameter, extending the function signature to: *function (config, result, vertex, path, edges)*. 

  Visitor functions are not expected to return values, but they may modify the *result* variable as 
  needed (e.g. by pushing vertex data into the result).
- *expander*: expander function that is responsible for returning edges and vertices
  directly connected to a vertex. The function signature is *function (config, vertex, path)*.
  The expander function is required to return an array of connection objects, consisting of an
  *edge* and *vertex* attribute each. If there are no connecting edges, the expander is expected to
  return an empty array.
- *filter*: vertex filter function. The function signature is *function (config, vertex, path)*. It
  may return one of the following values:
  - *undefined*: vertex will be included in the result and connected edges will be traversed
  - *"exclude"*: vertex will not be included in the result and connected edges will be traversed
  - *"prune"*: vertex will be included in the result but connected edges will not be traversed
  - [ *"prune"*, *"exclude"* ]: vertex will not be included in the result and connected edges will not
    be returned
- *expandFilter*: filter function applied on each edge/vertex combination determined by the expander.
  The function signature is *function (config, vertex, edge, path)*. The function should return
  *true* if the edge/vertex combination should be processed, and *false* if it should be ignored.
- *sort*: a filter function to determine the order in which connected edges are processed. The
  function signature is *function (l, r)*. The function is required to return one of the following
  values:
  - *-1* if *l* should have a sort value less than *r*
  - *1* if *l* should have a higher sort value than *r*
  - *0* if *l* and *r* have the same sort value
- *strategy*: determines the visitation strategy. Possible values are *depthfirst* and *breadthfirst*.
- *order*: determines the visitation order. Possible values are *preorder*, *postorder*, and
  *preorder-expander*. *preorder-expander* is the same as *preorder*, except that the signature of
  the *visitor* function will change as described above.
- *itemOrder*: determines the order in which connections returned by the expander will be processed. 
  Possible values are *forward* and *backward*.
- *maxDepth*: if set to a value greater than *0*, this will limit the traversal to this maximum depth. 
- *minDepth*: if set to a value greater than *0*, all vertices found on a level below the *minDepth*
  level will not be included in the result.
- *maxIterations*: the maximum number of iterations that the traversal is allowed to perform. It is
  sensible to set this number so unbounded traversals will terminate at some point.
- *uniqueness*: an object that defines how repeated visitations of vertices should be handled.
  The *uniqueness* object can have a sub-attribute *vertices*, and a sub-attribute *edges*. Each
  sub-attribute can have one of the following values:
  - *"none"*: no uniqueness constraints
  - *"path"*: element is excluded if it is already contained in the current path. This setting may be
    sensible for graphs that contain cycles (e.g. A → B → C → A).
  - *"global"*: element is excluded if it was already found/visited at any point during the traversal.
- *buildVertices*: this attribute controls whether vertices encountered during the traversal will be
  looked up in the database and will be made available to visitor, filter, and expandFilter functions.
  By default, vertices will be looked up and made available. However, there are some special use
  cases when fully constructing vertex objects is not necessary and can be avoided. For example, if
  a traversal is meant to only count the number of visited vertices but do not read any data from 
  vertices, this option might be set to *true*.
