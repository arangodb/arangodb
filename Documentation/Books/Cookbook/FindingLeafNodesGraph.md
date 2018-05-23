# Finding leaf nodes of a graph using AQL

## Problem

I want to traverse a graph with AQL and return all the leaf nodes.

## Solution

Use a traversal with a custom visitor function. This is available since ArangoDB 2.4.2.
A custom visitor function is user-defined function that is called for every node that
is visited during the traversal. It can append values to the traversal result.

User-defined visitor functions are written in JavaScript. Once the function is written,
it needs to be registered on the server to become usable from inside an AQL query.

### Example data setup

For the following, we need the example graph and data from [here](https://jsteemann.github.io/downloads/code/world-graph-setup.js).
Please download the code from the link and store it in the filesystem using a filename
of `world-graph-setup.js`. Then start the ArangoShell and run the code from the file:

```js
require("internal").load("/path/to/file/world-graph-setup.js");
```

The script will create the following two collections and load some data into them:

- `v`: a collection with vertex documents
- `e`: an edge collection containing the connections between vertices in `v`

The data in the example graph connects capitals to countries, countries to 
continents, and all continents to a single root node. The structure looks like
this:

```
root <--[is in]-- continent <--[is in]-- country <--[is in]-- capital
```

In this graph, we're only interested in the leaf nodes. While in this graph
they would be easy to find using their `type` value of `capital`, we are 
interested in a more general solution. In general, a leaf node is a node that
does not have any further edges connected.

We start our traversal at the root node, and from that vertex follow all incoming 
edges and vertices until we have visited all leaf nodes. The general traversal
framework will take care of all this, all we need to do is to tweak its 
configuration and behavior. The default visitor will put every visited vertex
into the result, and this is not what we want. Instead, we will only return
leaf nodes.

### Registering a custom visitor function

Here's a visitor function to do this. Please execute the following code in the
ArangoShell to register the function and make it available from with AQL:

```js
var aqlfunctions = require("org/arangodb/aql/functions");

aqlfunctions.register("myfunctions::leafNodeVisitor", function (config, result, vertex, path, connected) {
  if (connected && connected.length === 0) {
    return vertex.name + " (" + vertex.type + ")";
  }
});
```

That code snippet should have stored the custom visitor under a name 
`myfunctions::leafNodeVisitor`. 

### Using the visitor from an AQL query

Now we only need to write an AQL query that invokes the custom visitor. The following
query is an example for this. You can execute it from the **AQL editor** in the web interface:
 
```
LET params = { 
  order : "preorder-expander",
  visitor : "myfunctions::leafNodeVisitor", 
  visitorReturnsResults : true 
}
FOR result IN TRAVERSAL(v, e, "v/world", "inbound", params) 
  RETURN result
```

### Remarks

Please note the following things:

* the AQL query will use two collections as in the example graph. They are named `v` and 
  `e`. The vertices are connected via inbound edges. The traversal will start at the
  node with id `v/world` as specified. To use other collections, a different start vertex
  or outbound connections, please adjust the part of the query following `TRAVERSAL(...`.

* by default, custom visitor functions will be called **before** connecting edges are
  determined. Setting `order` to `preorder-expander` in the traversal configuration
  as above will lead to the visitor function being called **after** connecting edges
  have been determined. The connection info will then be passed into the visitor as
  a fifth parameter (named `connected` above)

* the visitor function name (`visitor` attribute of the configuration) in the example
  is `myfunctions::leafNodeVisitor`. Adjust as required!

* the `visitorReturnsResults` attribute was set to `true`. This means that the return
  value of the custom visitor function will be appended to the result of the `TRAVERSAL`
  function. In the custom visitor above, only leaf nodes will be returned. For every
  other node visited, the custom visitor will not return anything, so nothing will get
  append to the traversal result in that case.

### Further uses

The above example is using the AQL function `TRAVERSAL()`, which requires a vertex and 
an edge collection to be specified. Custom visitors can also be used in the
`GRAPH_TRAVERSAL()` AQL function, which does not require named collections but a named 
graph. To make the above query work on a graph `WorldGraph` and use `GRAPH_TRAVERSAL()`
instead of `TRAVERSAL()`, change the query to:

```
LET params = { 
  order : "preorder-expander",
  visitor : "myfunctions::leafNodeVisitor", 
  visitorReturnsResults : true 
}
FOR result IN GRAPH_TRAVERSAL("WorldGraph", "v/world", "inbound", params) 
  RETURN result
```

Custom visitors can be also combined with custom filter functions to restrict results to only
certain types of vertices, or to follow only specific edges/connections/paths in the graph.


**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #graph #traversal #aql
