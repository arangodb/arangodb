!CHAPTER Getting started

To use a traversal object, we first need to require the `traversal` module:
    
    var traversal = require("org/arangodb/graph/traversal");

We then need to setup a configuration for the traversal and determine at which vertex to 
start the traversal:
    
    var config = {
      datasource: traversal.collectionDatasourceFactory(db.e),
      strategy: "depthfirst",
      order: "preorder",
      filter: traversal.visitAllFilter,
      expander: traversal.inboundExpander,
      maxDepth: 1
    };
    
    var startVertex = db._document("v/world");

Note that the startVertex needs to be a document, not only a document id.
    
We can then create a traverser and start the traversal by calling its `traverse` method.
Note that `traverse` needs a `result` object, which it can modify in place:

    var result = { 
      visited: { 
        vertices: [ ], 
        paths: [ ] 
      } 
    };

    var traverser = new traversal.Traverser(config);
    traverser.traverse(result, startVertex);

Finally, we can print the contents of the `results` object, limited to the visited vertices.
We will only print the name and type of each visited vertex for brevity:

    require("internal").print(result.visited.vertices.map(function(vertex) { 
      return vertex.name + " (" + vertex.type + ")"; 
    }));
    
    
The full script, which includes all steps carried out so far is thus:

    var traversal = require("org/arangodb/graph/traversal");

    var config = {
      datasource: traversal.collectionDatasourceFactory(db.e),
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
    
    require("internal").print(result.visited.vertices.map(function(vertex) { 
      return vertex.name + " (" + vertex.type + ")"; 
    }));

The result is a list of vertices that were visited during the traversal, starting at the
start vertex (i.e. `v/world` in our example):

    [ 
      "World (root)", 
      "Africa (continent)", 
      "Asia (continent)", 
      "Australia (continent)", 
      "Europe (continent)", 
      "North America (continent)", 
      "South America (continent)" 
    ]

Note that the result is limited to vertices directly connected to the start vertex. We
achieved this by setting the `maxDepth` attribute to `1`. Not setting it would return the
full list of vertices.

!SUBSECTION Traversal Direction

For the examples contained in this manual, we'll be starting the traversals at vertex
`v/world`. Vertices in our graph are connected like this:

  v/world <- is-in <- continent (Africa) <- is-in <- country (Algeria) <- is-in <- capital (Algiers)

To get any meaningful results, we must traverse the graph in inbound order. This means,
we'll be following all incoming edges of to a vertex. In the traversal configuration, we
have specified this via the `expander` attribute:

    var config = {
      ...
      expander: traversal.inboundExpander
    };

For other graphs, we might want to traverse via the outgoing edges. For this, we can
use the `outboundExpander`. There is also an `anyExpander`, which will follow both outgoing
and incoming edges. This should be used with care and the traversal should always be
limited to a maximum number of iterations (e.g. using the `maxIterations` attribute) in
order to terminate at some point.
    
To invoke the default outbound expander for a graph, simply use the predefined function:

    var config = {
      ...
      expander: traversal.outboundExpander
    };

Please note the outbound expander will not produce any output for the examples if we still
start the traversal at the `v/world` vertex.

Still, we can use the outbound expander if we start somewhere else in the graph, e.g. 
    
    var traversal = require("org/arangodb/graph/traversal");

    var config = {
      datasource: traversal.collectionDatasourceFactory(db.e),
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
    
    require("internal").print(result.visited.vertices.map(function(vertex) { 
      return vertex.name + " (" + vertex.type + ")"; 
    }));

The result is:

    [ 
      "Algiers (capital)", 
      "Algeria (country)", 
      "Africa (continent)", 
      "World (root)" 
    ]

which confirms that now we're going outbound.

!SUBSECTION Traversal Strategy

!SUBSUBSECTION Depth-first traversals

The visitation order of vertices is determined by the `strategy`, `order` attributes set
in the configuration. We chose `depthfirst` and `preorder`, meaning the traverser will 
emit each vertex before handling connected edges (pre-order), and descend into any 
connected edges before processing other vertices on the same level (depth-first). 

Let's remove the `maxDepth` attribute now. We'll now be getting all vertices (directly
and indirectly connected to the start vertex):

    var config = {
      datasource: traversal.collectionDatasourceFactory(db.e),
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

    require("internal").print(result.visited.vertices.map(function(vertex) { 
      return vertex.name + " (" + vertex.type + ")"; 
    }));

The result will be a longer list, assembled in depth-first, pre-order order. For 
each continent found, the traverser will descend into linked countries, and then into
the linked capital:

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

Let's switch the `order` attribute from `preorder` to `postorder`. This will make the
traverser emit vertices after all connected vertices were visited (i.e. most distant
vertices will be emitted first):

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

!SUBSUBSECTION Breadth-first traversals

If we go back to `preorder`, but change the strategy to `breadth-first` and re-run the 
traversal, we'll see that the return order changes, and items on the same level will be 
returned adjacently:

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

Note that the order of items returned for the same level is undefined.
This is because there is no natural order of edges for a vertex with
multiple connected edges. To explicitly set the order for edges on the
same level, you can specify an edge comparator function with the `sort`
attribute:
    
    var config = {
      ...
      sort: function (l, r) { return l._key < r._key ? 1 : -1; }
      ...
    };
 
The arguments l and r are edge documents.
This will traverse edges of the same vertex in backward `_key` order:

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

Note that this attribute only works for the usual expanders
`traversal.inboundExpander`, `traversal.outboundExpander`,
`traversal.anyExpander` and their corresponding "WithLabels" variants.
If you are using custom expanders (see @ref TraversalsCustomExpanders)
you have to organise the sorting within the specified expander.


!SUBSUBSECTION Writing Custom Visitors

So far we have used much of the traverser's default functions. The traverser is very
configurable and many of the default functions can be overridden with custom functionality.

For example, we have been using the default visitor function (which is always used if
the configuration does not contain the `visitor` attribute). The default visitor function
is called for each vertex in a traversal, and will push it into the result.
This is the reason why the `result` variable looked different after the traversal, and
needed to be initialized before the traversal was started.

We can write our own visitor function if we want to. The general function signature for
visitor function is as follows:

    var config = {
      ...
      visitor: function (config, result, vertex, path) { ... }
    };

Visitor functions are not expected to return any values. Instead, they can modify the
`result` variable (e.g. by pushing the current vertex into it), or do anything else.
For example, we can create a simple visitor function that only prints information about
the current vertex as we traverse:

    var config = {
      datasource: traversal.collectionDatasourceFactory(db.e),
      strategy: "depthfirst",
      order: "preorder",
      filter: traversal.visitAllFilter,
      expander: traversal.inboundExpander,
      visitor: function (config, result, vertex, path) {
        require("internal").print("visiting vertex", vertex.name);
      }
    };
    
    var traverser = new traversal.Traverser(config);
    traverser.traverse(undefined, startVertex);


!SUBSECTION Filtering Vertices and Edges

!SUBSUBSECTION Filtering Vertices

So far we have returned all vertices that were visited during the traversal. This is not 
always required. If the result shall be restrict to just specific vertices, we can use a
filter function for vertices. It can be defined by setting the `filter` attribute of a
traversal configuration, e.g.:

    var config = {
      filter: function (config, vertex, path) {
        if (vertex.type !== 'capital') {
          return 'exclude';
        }
      }
    }

The above filter function will exclude all vertices that do not have a `type` value of
`capital`. The filter function will be called for each vertex found during the traversal.
It will receive the traversal configuration, the current vertex, and the full path from 
the traversal start vertex to the current vertex. The path consists of a list of edges, 
and a list of vertices. We could also filter everything but capitals by checking the
length of the path from the start vertex to the current vertex. Capitals will have a
distance of 3 from the `v/world` start vertex 
(capital -> is-in -> country -> is-in -> continent -> is-in -> world):
    
    var config = {
      ...
      filter: function (config, vertex, path) {
        if (path.edges.length < 3) {
          return 'exclude';
        }
      }
    }

Note that if a filter function returns nothing (or `undefined`), the current vertex
will be included, and all connected edges will be followed. If a filter function
returns `exclude` the current vertex will be excluded from the result, and all still
all connected edges will be followed. If a filter function returns `prune`, the
current vertex will be included, but no connected edges will be followed.
    
For example, the following filter function will not descend into connected edges of
continents, limiting the depth of the traversal. Still, continent vertices will be
included in the result:

    var config = {
      ...
      filter: function (config, vertex, path) {
        if (vertex.type === 'continent') {
          return 'prune';
        }
      }
    }

It is also possible to combine `exclude` and `prune` by returning a list with both
values:

    return [ 'exclude', 'prune' ];

!SUBSECTION Filtering Edges

It is possible to exclude certain edges from the traversal. To filter on edges, a
filter function can be defined via the `expandFilter` attribute. The `expandFilter`
is a function which is called for each edge during a traversal.

It will receive the current edge (`edge` variable) and the vertex which the edge
connects to (in the direction of the traversal). It also receives the current path
from the start vertex up to the current vertex (excluding the current edge and the
vertex the edge points to).

If the function returns `true`, the edge will be followed. If the function returns
`false`, the edge will not be followed.
Here is a very simple custom edge filter function implementation, which simply 
includes edges if the (edges) path length is less than 1, and will exclude any 
other edges. This will effectively terminate the traversal after the first level 
of edges:

    var config = {
      ...
      expandFilter: function (config, vertex, edge, path) {
        return (path.edges.length < 1);
      }
    };

!SUBSECTION Writing Custom Expanders


The edges connected to a vertex are determined by the expander. So far we have used a 
default expander (the default inbound expander to be precise). The default inbound 
expander simply enumerates all connected ingoing edges for a vertex, based on the
edge collection specified in the traversal configuration.

There is also a default outbound expander, which will enumerate all connected outgoing
edges. Finally, there is an any expander, which will follow both ingoing and outgoing
edges.

If connected edges must be determined in some different fashion for whatever reason, a
custom expander can be written and registered by setting the `expander` attribute of the 
configuration. The expander function signature is as follows:

    var config = {
      ...
      expander: function (config, vertex, path) { ... }
    }

It is the expander's responsibility to return all edges and vertices directly
connected to the current vertex (which is passed via the `vertex` variable). 
The full path from the start vertex up to the current vertex is also supplied via
the `path` variable.
An expander is expected to return a list of objects, which need to have an `edge`
and a `vertex` attribute each.

Note that if you want to rely on a particular order in which the edges
are traversed, you have to sort the edges returned by your expander
within the code of the expander. The functions to get outbound, inbound
or any edges from a vertex do not guarantee any particular order!

A custom implementation of an inbound expander could look like this (this is a
non-deterministic expander, which randomly decides whether or not to include
connected edges):
  
    var config = {
      ...
      expander: function (config, vertex, path) {
        var connections = [ ];
        db.e.inEdges(vertex._id).forEach(function (edge) {
          if (Math.random() >= 0.5) {
            connections.push({ edge: edge, vertex: db._document(edge._from) });
          }
        });

        return connections;
      } 
    };

A custom expander can also be used as an edge filter because it has full control
over which edges will be returned.

Following are two examples of custom expanders that pick edges based on attributes
of the edges and the connected vertices.

Finding the connected edges / vertices based on an attribute `when` in the 
connected vertices. The goal is to follow the edge that leads to the vertex
with the highest value in the `when` attribute:
 
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

Finding the connected edges / vertices based on an attribute `when` in the 
edge itself. The goal is to pick the one edge (out of potentially many) that
has the highest `when` attribute value:

    var config = {
      ...
      expander: function (config, vertex, path) {
        var datasource = config.datasource;
        // determine all outgoing edges
        var outEdges = datasource.getOutEdges(vertex);
     
        if (outEdges.length === 0) {
          return [ ]; // return an empty list
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


!SUBSECTION Configuration Overview

This section summarizes the configuration attributes for the traversal object. The
configuration can consist of the following attributes:

- `visitor`: visitor function for vertices. The function signature is `function (config, result, vertex, path)`.
   This function is not expected to return a value, but may modify the `variable` as needed
   (e.g. by pushing vertex data into the result).
- `expander`: expander function that is responsible for returning edges and vertices
   directly connected to a vertex . The function signature is `function (config, vertex, path)`.
   The expander function is required to return a list of connection objects, consisting of an
   `edge` and `vertex` attribute each.
- `filter`: vertex filter function. The function signature is `function (config, vertex, path)`. It
   may return one of the following values:
   - `undefined`: vertex will be included in the result and connected edges will be traversed
   - `exclude`: vertex will not be included in the result and connected edges will be traversed
   - `prune`: vertex will be included in the result but connected edges will not be traversed
   - [ `prune`, `exclude` ]: vertex will not be included in the result and connected edges will not
     be returned
- `expandFilter`: filter function applied on each edge/vertex combination determined by the expander.
  The function signature is `function (config, vertex, edge, path)`. The function should return
  `true` if the edge/vertex combination should be processed, and `false` if it should be ignored.
- `sort`: a filter function to determine the order in which connected edges are procssed. The
  function signature is `function (l, r)`. The function is required to return one of the following
  values:
  - `-1` if `l` should have a sort value less than `r`
  - `1` if `l` should have a higher sort value than `r`
  - `0` if `l` and `r` have the same sort value
- `strategy`: determines the visitation strategy. Possible values are `depthfirst` and `breadthfirst`.
- `order`: determines the visitation order. Possible values are `preorder` and `postorder`.
- `itemOrder`: determines the order in which connections returned by the expander will be processed. 
  Possible values are `forward` and `backward`.
- `maxDepth`: if set to a value greater than `0`, this will limit the traversal to this maximum depth. 
- `minDepth`: if set to a value greater than `0`, all vertices found on a level below the `minDepth`
  level will not be included in the result.
- `maxIterations`: the maximum number of iterations that the traversal is allowed to perform. It is
  sensible to set this number so unbounded traversals will terminate at some point.
- `uniqueness`: an object that defines how repeated visitations of vertices should be handled.
  The `uniqueness` object can have a sub-attribute `vertices`, and a sub-attribute `edges`. Each
  sub-attribute can have one of the following values:
  - `none`: no uniqueness constraints
  - `path`: element is excluded if it is already contained in the current path. This setting may be
    sensible for graphs that contain cycles (e.g. A -> B -> C -> A).
  - `global`: element is excluded if it was already found/visited at any point during the traversal.

