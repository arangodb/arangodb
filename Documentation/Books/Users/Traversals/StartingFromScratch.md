!CHAPTER Starting from Scratch

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

