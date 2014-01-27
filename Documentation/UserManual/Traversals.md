Traversals {#Traversals}
========================

@NAVIGATE_Traversals
@EMBEDTOC{TraversalsTOC}

Introduction{#TraversalsIntroduction}
=====================================

ArangoDB provides several ways to query graph data. Very simple operations can
be composed with the low-level edge methods `edges`, `inEdges`, and `outEdges` for
edge collections (see @ref HandlingEdgesShell). For more complex operations,
ArangoDB provides predefined traversal objects.

For any of the following examples, we'll be using the example collections `v` and `e`,
populated with continents, countries and capitals data listed below (see @ref TraversalsExampleData).

Starting from Scratch{#TraversalsManual}
========================================

ArangoDB provides the `edges`, `inEdges`, and `outEdges` methods for edge collections.
These methods can be used to quickly determine if a vertex is connected to other vertices,
and which.
This functionality can be exploited to write very simple graph queries in JavaScript.

For example, to determine which edges are linked to the `world` vertex, we can use `inEdges`:

    db.e.inEdges('v/world').forEach(function(edge) { 
      require("internal").print(edge._from, "->", edge.type, "->", edge._to); 
    });

`inEdges` will give us all ingoing edges for the specified vertex `v/world`. The result
is a JavaScript list, that we can iterate over and print the results:

    v/continent-africa -> is-in -> v/world
    v/continent-south-america -> is-in -> v/world
    v/continent-asia -> is-in -> v/world
    v/continent-australia -> is-in -> v/world
    v/continent-europe -> is-in -> v/world
    v/continent-north-america -> is-in -> v/world

Note that `edges`, `inEdges`, and `outEdges` return a list of edges. If we want to retrieve
the linked vertices, we can use each edges' `_from` and `_to` attributes as follows:

    db.e.inEdges('v/world').forEach(function(edge) { 
      require("internal").print(db._document(edge._from).name, "->", edge.type, "->", db._document(edge._to).name); 
    });

We are using the `document` method from the `db` object to retrieve the connected vertices now.

While this may be sufficient for one-level graph operations, writing a traversal by hand
may become too complex for multi-level traversals.

Using Traversal Objects{#TraversalsObject}
==========================================

ArangoDB provides predefined traversal objects that can be used to run a multi-level traversal
on a graph. To use the traversal objects, it is required that all edges of the graph are
stored in a single edge collection.

Getting started{#TraversalsObjectGettingStarted}
------------------------------------------------

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

Traversal Direction{#TraversalsObjectDirection}
-----------------------------------------------

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

Traversal Strategy{#TraversalsObjectStrategy}
---------------------------------------------

###Depth-first traversals

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

###Breadth-first traversals

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

Note that the order of items returned for the same level is undefined. This is because there
is no natural order of edges for a vertex with multiple connected edges.
To explicitly set the order for edges on the same level, you can specify an edge comparator
function with the `sort` attribute:
    
    var config = {
      ...
      sort: function (l, r) { return l._key < r._key ? 1 : -1; }
      ...
    };
 
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

Writing Custom Visitors{#TraversalsObjectVisitors}
--------------------------------------------------

So far we have used much of the traverser's default functions. The traverser is very
configurable and many of the default functions can be overriden with custom functionality.

For example, we have been using the default visitor function (which is always used if
the configuration does not contain the `visitor` attribute). The default visitor function
is called for each vertex in a traversal, and will push it into the result.
This is the reason why the `result` variable looked different after the traversal, and
needed to be initialised before the traversal was started.

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


Filtering Vertices and Edges{#TraversalsObjectFilters}
------------------------------------------------------

Filtering Vertices
------------------

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

Filtering Edges
---------------

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

Writing Custom Expanders
------------------------

The edges connected to a vertex are determined by the expander. So far we have used a 
default expander (the default inbound expander to be precise). The default inbound 
expander simply enumerates all connected ingoing edges for a vertex, based on the
edge collection specified in the traversal configuration.

There is also a default outbound expander, which will enumerate all connected outgoing
edges. Finally, there is an anyexpander, which will follow both ingoing and outgoing
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

Configuration Overview{#TraversalsObjectConfiguration}
------------------------------------------------------

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

Example Data{#TraversalsExampleData}
====================================

The above examples all use a vertex collection `e` and an edge collection `e`. The vertex
collection `v` contains continents, countries, and captials. The edge collection `e` 
contains connections between continents and countries, and between countries and captials.

To set up the collections and populate them with initial data, the following script was used:

    db._create("v");
    db._createEdgeCollection("e");

    // vertices: root node 
    db.v.save({ _key: "world", name: "World", type: "root" });

    // vertices: continents 
    db.v.save({ _key: "continent-africa", name: "Africa", type: "continent" });
    db.v.save({ _key: "continent-asia", name: "Asia", type: "continent" });
    db.v.save({ _key: "continent-australia", name: "Australia", type: "continent" });
    db.v.save({ _key: "continent-europe", name: "Europe", type: "continent" });
    db.v.save({ _key: "continent-north-america", name: "North America", type: "continent" });
    db.v.save({ _key: "continent-south-america", name: "South America", type: "continent" });

    // vertices: countries 
    db.v.save({ _key: "country-afghanistan", name: "Afghanistan", type: "country", code: "AFG" });
    db.v.save({ _key: "country-albania", name: "Albania", type: "country", code: "ALB" });
    db.v.save({ _key: "country-algeria", name: "Algeria", type: "country", code: "DZA" });
    db.v.save({ _key: "country-andorra", name: "Andorra", type: "country", code: "AND" });
    db.v.save({ _key: "country-angola", name: "Angola", type: "country", code: "AGO" });
    db.v.save({ _key: "country-antigua-and-barbuda", name: "Antigua and Barbuda", type: "country", code: "ATG" });
    db.v.save({ _key: "country-argentina", name: "Argentina", type: "country", code: "ARG" });
    db.v.save({ _key: "country-australia", name: "Australia", type: "country", code: "AUS" });
    db.v.save({ _key: "country-austria", name: "Austria", type: "country", code: "AUT" });
    db.v.save({ _key: "country-bahamas", name: "Bahamas", type: "country", code: "BHS" });
    db.v.save({ _key: "country-bahrain", name: "Bahrain", type: "country", code: "BHR" });
    db.v.save({ _key: "country-bangladesh", name: "Bangladesh", type: "country", code: "BGD" });
    db.v.save({ _key: "country-barbados", name: "Barbados", type: "country", code: "BRB" });
    db.v.save({ _key: "country-belgium", name: "Belgium", type: "country", code: "BEL" });
    db.v.save({ _key: "country-bhutan", name: "Bhutan", type: "country", code: "BTN" });
    db.v.save({ _key: "country-bolivia", name: "Bolivia", type: "country", code: "BOL" });
    db.v.save({ _key: "country-bosnia-and-herzegovina", name: "Bosnia and Herzegovina", type: "country", code: "BIH" });
    db.v.save({ _key: "country-botswana", name: "Botswana", type: "country", code: "BWA" });
    db.v.save({ _key: "country-brazil", name: "Brazil", type: "country", code: "BRA" });
    db.v.save({ _key: "country-brunei", name: "Brunei", type: "country", code: "BRN" });
    db.v.save({ _key: "country-bulgaria", name: "Bulgaria", type: "country", code: "BGR" });
    db.v.save({ _key: "country-burkina-faso", name: "Burkina Faso", type: "country", code: "BFA" });
    db.v.save({ _key: "country-burundi", name: "Burundi", type: "country", code: "BDI" });
    db.v.save({ _key: "country-cambodia", name: "Cambodia", type: "country", code: "KHM" });
    db.v.save({ _key: "country-cameroon", name: "Cameroon", type: "country", code: "CMR" });
    db.v.save({ _key: "country-canada", name: "Canada", type: "country", code: "CAN" });
    db.v.save({ _key: "country-chad", name: "Chad", type: "country", code: "TCD" });
    db.v.save({ _key: "country-chile", name: "Chile", type: "country", code: "CHL" });
    db.v.save({ _key: "country-colombia", name: "Colombia", type: "country", code: "COL" });
    db.v.save({ _key: "country-cote-d-ivoire", name: "Cote d'Ivoire", type: "country", code: "CIV" });
    db.v.save({ _key: "country-croatia", name: "Croatia", type: "country", code: "HRV" });
    db.v.save({ _key: "country-czech-republic", name: "Czech Republic", type: "country", code: "CZE" });
    db.v.save({ _key: "country-denmark", name: "Denmark", type: "country", code: "DNK" });
    db.v.save({ _key: "country-ecuador", name: "Ecuador", type: "country", code: "ECU" });
    db.v.save({ _key: "country-egypt", name: "Egypt", type: "country", code: "EGY" });
    db.v.save({ _key: "country-eritrea", name: "Eritrea", type: "country", code: "ERI" });
    db.v.save({ _key: "country-finland", name: "Finland", type: "country", code: "FIN" });
    db.v.save({ _key: "country-france", name: "France", type: "country", code: "FRA" });
    db.v.save({ _key: "country-germany", name: "Germany", type: "country", code: "DEU" });
    db.v.save({ _key: "country-people-s-republic-of-china", name: "People's Republic of China", type: "country", code: "CHN" });

    // vertices: capitals 
    db.v.save({ _key: "capital-algiers", name: "Algiers", type: "capital" });
    db.v.save({ _key: "capital-andorra-la-vella", name: "Andorra la Vella", type: "capital" });
    db.v.save({ _key: "capital-asmara", name: "Asmara", type: "capital" });
    db.v.save({ _key: "capital-bandar-seri-begawan", name: "Bandar Seri Begawan", type: "capital" });
    db.v.save({ _key: "capital-beijing", name: "Beijing", type: "capital" });
    db.v.save({ _key: "capital-berlin", name: "Berlin", type: "capital" });
    db.v.save({ _key: "capital-bogota", name: "Bogota", type: "capital" });
    db.v.save({ _key: "capital-brasilia", name: "Brasilia", type: "capital" });
    db.v.save({ _key: "capital-bridgetown", name: "Bridgetown", type: "capital" });
    db.v.save({ _key: "capital-brussels", name: "Brussels", type: "capital" });
    db.v.save({ _key: "capital-buenos-aires", name: "Buenos Aires", type: "capital" });
    db.v.save({ _key: "capital-bujumbura", name: "Bujumbura", type: "capital" });
    db.v.save({ _key: "capital-cairo", name: "Cairo", type: "capital" });
    db.v.save({ _key: "capital-canberra", name: "Canberra", type: "capital" });
    db.v.save({ _key: "capital-copenhagen", name: "Copenhagen", type: "capital" });
    db.v.save({ _key: "capital-dhaka", name: "Dhaka", type: "capital" });
    db.v.save({ _key: "capital-gaborone", name: "Gaborone", type: "capital" });
    db.v.save({ _key: "capital-helsinki", name: "Helsinki", type: "capital" });
    db.v.save({ _key: "capital-kabul", name: "Kabul", type: "capital" });
    db.v.save({ _key: "capital-la-paz", name: "La Paz", type: "capital" });
    db.v.save({ _key: "capital-luanda", name: "Luanda", type: "capital" });
    db.v.save({ _key: "capital-manama", name: "Manama", type: "capital" });
    db.v.save({ _key: "capital-nassau", name: "Nassau", type: "capital" });
    db.v.save({ _key: "capital-n-djamena", name: "N'Djamena", type: "capital" });
    db.v.save({ _key: "capital-ottawa", name: "Ottawa", type: "capital" });
    db.v.save({ _key: "capital-ouagadougou", name: "Ouagadougou", type: "capital" });
    db.v.save({ _key: "capital-paris", name: "Paris", type: "capital" });
    db.v.save({ _key: "capital-phnom-penh", name: "Phnom Penh", type: "capital" });
    db.v.save({ _key: "capital-prague", name: "Prague", type: "capital" });
    db.v.save({ _key: "capital-quito", name: "Quito", type: "capital" });
    db.v.save({ _key: "capital-saint-john-s", name: "Saint John's", type: "capital" });
    db.v.save({ _key: "capital-santiago", name: "Santiago", type: "capital" });
    db.v.save({ _key: "capital-sarajevo", name: "Sarajevo", type: "capital" });
    db.v.save({ _key: "capital-sofia", name: "Sofia", type: "capital" });
    db.v.save({ _key: "capital-thimphu", name: "Thimphu", type: "capital" });
    db.v.save({ _key: "capital-tirana", name: "Tirana", type: "capital" });
    db.v.save({ _key: "capital-vienna", name: "Vienna", type: "capital" });
    db.v.save({ _key: "capital-yamoussoukro", name: "Yamoussoukro", type: "capital" });
    db.v.save({ _key: "capital-yaounde", name: "Yaounde", type: "capital" });
    db.v.save({ _key: "capital-zagreb", name: "Zagreb", type: "capital" });

    // edges: continent -> world 
    db.e.save("v/continent-africa", "v/world", { type: "is-in" });
    db.e.save("v/continent-asia", "v/world", { type: "is-in" });
    db.e.save("v/continent-australia", "v/world", { type: "is-in" });
    db.e.save("v/continent-europe", "v/world", { type: "is-in" });
    db.e.save("v/continent-north-america", "v/world", { type: "is-in" });
    db.e.save("v/continent-south-america", "v/world", { type: "is-in" });

    // edges: country -> continent 
    db.e.save("v/country-afghanistan", "v/continent-asia", { type: "is-in" });
    db.e.save("v/country-albania", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-algeria", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-andorra", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-angola", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-antigua-and-barbuda", "v/continent-north-america", { type: "is-in" });
    db.e.save("v/country-argentina", "v/continent-south-america", { type: "is-in" });
    db.e.save("v/country-australia", "v/continent-australia", { type: "is-in" });
    db.e.save("v/country-austria", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-bahamas", "v/continent-north-america", { type: "is-in" });
    db.e.save("v/country-bahrain", "v/continent-asia", { type: "is-in" });
    db.e.save("v/country-bangladesh", "v/continent-asia", { type: "is-in" });
    db.e.save("v/country-barbados", "v/continent-north-america", { type: "is-in" });
    db.e.save("v/country-belgium", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-bhutan", "v/continent-asia", { type: "is-in" });
    db.e.save("v/country-bolivia", "v/continent-south-america", { type: "is-in" });
    db.e.save("v/country-bosnia-and-herzegovina", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-botswana", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-brazil", "v/continent-south-america", { type: "is-in" });
    db.e.save("v/country-brunei", "v/continent-asia", { type: "is-in" });
    db.e.save("v/country-bulgaria", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-burkina-faso", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-burundi", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-cambodia", "v/continent-asia", { type: "is-in" });
    db.e.save("v/country-cameroon", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-canada", "v/continent-north-america", { type: "is-in" });
    db.e.save("v/country-chad", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-chile", "v/continent-south-america", { type: "is-in" });
    db.e.save("v/country-colombia", "v/continent-south-america", { type: "is-in" });
    db.e.save("v/country-cote-d-ivoire", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-croatia", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-czech-republic", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-denmark", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-ecuador", "v/continent-south-america", { type: "is-in" });
    db.e.save("v/country-egypt", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-eritrea", "v/continent-africa", { type: "is-in" });
    db.e.save("v/country-finland", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-france", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-germany", "v/continent-europe", { type: "is-in" });
    db.e.save("v/country-people-s-republic-of-china", "v/continent-asia", { type: "is-in" });

    // edges: capital -> country 
    db.e.save("v/capital-algiers", "v/country-algeria", { type: "is-in" });
    db.e.save("v/capital-andorra-la-vella", "v/country-andorra", { type: "is-in" });
    db.e.save("v/capital-asmara", "v/country-eritrea", { type: "is-in" });
    db.e.save("v/capital-bandar-seri-begawan", "v/country-brunei", { type: "is-in" });
    db.e.save("v/capital-beijing", "v/country-people-s-republic-of-china", { type: "is-in" });
    db.e.save("v/capital-berlin", "v/country-germany", { type: "is-in" });
    db.e.save("v/capital-bogota", "v/country-colombia", { type: "is-in" });
    db.e.save("v/capital-brasilia", "v/country-brazil", { type: "is-in" });
    db.e.save("v/capital-bridgetown", "v/country-barbados", { type: "is-in" });
    db.e.save("v/capital-brussels", "v/country-belgium", { type: "is-in" });
    db.e.save("v/capital-buenos-aires", "v/country-argentina", { type: "is-in" });
    db.e.save("v/capital-bujumbura", "v/country-burundi", { type: "is-in" });
    db.e.save("v/capital-cairo", "v/country-egypt", { type: "is-in" });
    db.e.save("v/capital-canberra", "v/country-australia", { type: "is-in" });
    db.e.save("v/capital-copenhagen", "v/country-denmark", { type: "is-in" });
    db.e.save("v/capital-dhaka", "v/country-bangladesh", { type: "is-in" });
    db.e.save("v/capital-gaborone", "v/country-botswana", { type: "is-in" });
    db.e.save("v/capital-helsinki", "v/country-finland", { type: "is-in" });
    db.e.save("v/capital-kabul", "v/country-afghanistan", { type: "is-in" });
    db.e.save("v/capital-la-paz", "v/country-bolivia", { type: "is-in" });
    db.e.save("v/capital-luanda", "v/country-angola", { type: "is-in" });
    db.e.save("v/capital-manama", "v/country-bahrain", { type: "is-in" });
    db.e.save("v/capital-nassau", "v/country-bahamas", { type: "is-in" });
    db.e.save("v/capital-n-djamena", "v/country-chad", { type: "is-in" });
    db.e.save("v/capital-ottawa", "v/country-canada", { type: "is-in" });
    db.e.save("v/capital-ouagadougou", "v/country-burkina-faso", { type: "is-in" });
    db.e.save("v/capital-paris", "v/country-france", { type: "is-in" });
    db.e.save("v/capital-phnom-penh", "v/country-cambodia", { type: "is-in" });
    db.e.save("v/capital-prague", "v/country-czech-republic", { type: "is-in" });
    db.e.save("v/capital-quito", "v/country-ecuador", { type: "is-in" });
    db.e.save("v/capital-saint-john-s", "v/country-antigua-and-barbuda", { type: "is-in" });
    db.e.save("v/capital-santiago", "v/country-chile", { type: "is-in" });
    db.e.save("v/capital-sarajevo", "v/country-bosnia-and-herzegovina", { type: "is-in" });
    db.e.save("v/capital-sofia", "v/country-bulgaria", { type: "is-in" });
    db.e.save("v/capital-thimphu", "v/country-bhutan", { type: "is-in" });
    db.e.save("v/capital-tirana", "v/country-albania", { type: "is-in" });
    db.e.save("v/capital-vienna", "v/country-austria", { type: "is-in" });
    db.e.save("v/capital-yamoussoukro", "v/country-cote-d-ivoire", { type: "is-in" });
    db.e.save("v/capital-yaounde", "v/country-cameroon", { type: "is-in" });
    db.e.save("v/capital-zagreb", "v/country-croatia", { type: "is-in" });

@BNAVIGATE_Traversals
