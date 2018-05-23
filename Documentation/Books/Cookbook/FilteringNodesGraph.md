# Filtering nodes in a graph using AQL

## Problem

I want to run a query on a graph using AQL, but only want to include certain vertices
or follow specific edges of the graph.

## Solution

Use a traversal with a custom filter function. A custom filter function is user-defined 
function that is called for every node that is encountered during a traversal.
The purpose of the filter function is to control if a given vertex is going to be
visited (i.e. if the traversal's visitor function should be called for it) and if its
connections/edges should be followed. A filter function can be used to skip non-interesting 
vertices early, providing a good way to make traversals more efficient or reduce their
result size.

### Example data setup

We first need some data that we can filter from. I suggest using the example graph and data 
from [here](https://jsteemann.github.io/downloads/code/world-graph-setup.js).
Please download the code from the link and store it in the filesystem using a filename
of `world-graph-setup.js`. Then start the ArangoShell and run the code from the file:

```js
require("internal").load("/path/to/file/world-graph-setup.js");
```

The data in the example graph connects capitals to countries, countries to 
continents, and all continents to a single root node. The structure looks like
this:

```
root <--[is in]-- continent <--[is in]-- country <--[is in]-- capital
```


### Registering a custom filter function

User-defined filter functions are written in JavaScript. Once a function is written,
it needs to be registered on the server to become usable from inside an AQL query.

The following snippet, to be executed in the ArangoShell, registers a custom filter function
named `myfunctions::europeFilter`:

```js
var aqlfunctions = require("org/arangodb/aql/functions");

aqlfunctions.register("myfunctions::europeFilter", function (config, vertex, path) {
  if (vertex.type === "country") {
    return "prune";
  }

  if (vertex.type === "continent") {
    if (vertex.name !== "Europe") {
      return [ "prune", "exclude" ];
    }
  }
 
  return "exclude"; 
});
```

The filter function inspects the `type` attribute of a vertex, and then decides what to 
do with it:

* it will not follow any connections of vertices with type `country`. It does so by returning
  `"prune"`

* it will not follow any connections of vertices with type `continent`, except if the vertex'
  `name` attribute contains the string `"Europe"`. For such vertices, it will return the
  array `[ "prune", "exclude" ]`, which tells the traversal to not follow any further connections
  of the vertex, and also to not visit the vertex.

* for everything else, including the vertex with type `continent` and name `Europe`, it will
  return `"exclude"`, which means to follow the vertex' connections but not call the visitor
  function for it.

### Invoking the filter function from AQL

Let's run a query now using the custom filter! I suggest running this query from the **AQL editor**
in the web interface:
 
```
LET params = { 
  filterVertices : "myfunctions::europeFilter"
}
FOR result IN TRAVERSAL(v, e, "v/world", "inbound", params) 
  RETURN { 
    name: result.vertex.name, 
    type: result.vertex.type 
  }
```

As we can see, only vertices of type `country` have been returned. This is because the filter
returned `"exclude"` for everything but vertices of type `"country"`. `"exclude"` means that the
visitor will not be called for the vertex. As the default functionality of a visitor is to put the
visited vertex into the result, it is now clear why the result does only contain nodes of type
`"country"`.

However, the result only contains European countries. This is because in the filter pruned all
connections for continents other than with name `Europe`. As a consequence, when visiting vertices
of type `continent`, the traversal only continent with the one continent node named `Europe`, but
did not descend into the others.

### Further uses
 
The above example is using the AQL function `TRAVERSAL()`, which requires a vertex and 
an edge collection to be specified. Custom visitors can also be used in the
`GRAPH_TRAVERSAL()` AQL function, which does not require named collections but a named 
graph. To make the above query work on a graph `WorldGraph` and use `GRAPH_TRAVERSAL()`
instead of `TRAVERSAL()`, change the query to:

```
LET params = { 
  filterVertices : "myfunctions::europeFilter"
}
FOR result IN GRAPH_TRAVERSAL("WorldGraph", "v/world", "inbound", params) 
  RETURN {
    name: result.vertex.name, 
    type: result.vertex.type 
  }
```

Custom filters can be combined with custom visitor functions for maximum flexibility.

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #graph #traversal #aql
