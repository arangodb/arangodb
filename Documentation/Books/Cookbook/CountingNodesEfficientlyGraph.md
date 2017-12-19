# Efficiently counting nodes in a graph using AQL

## Problem

I want to efficiently count the number of nodes in a graph that satisfy some filter
criteria with AQL.

## Solution

Use a traversal with a custom visitor function. This is available since ArangoDB 2.4.2.
A custom visitor function is user-defined function that is called for every node that
is visited during the traversal. It can append values to the traversal result.

Why do we need a custom visitor function for such a simple task as counting nodes?
The main reason is efficiency. The general traversal framework is very versatile and
can be used for different tasks. By default, it will copy the vertex data and the full
path information for each visited vertex into the result. This can easily get very big
for bigger graphs.

If all we want is to count nodes, we can be much more efficient by employing a custom
visitor function.

### Registering a custom counter function

User-defined visitor functions are written in JavaScript. Once a function is written,
it needs to be registered on the server to become usable from inside an AQL query.

The following snippet, to be executed in the ArangoShell, registers a custom visitor 
function that counts the number of nodes visited during a traversal:

```js
var aqlfunctions = require("org/arangodb/aql/functions");

aqlfunctions.register("myfunctions::vertexCounter", function (config, result) {
  if (result.length === 0) {
    result.push(0);
  }
  result[0]++;
});
```

The above registered the custom visitor under the name `myfunctions::vertexCounter`. It
can now be used from an AQL query that contains a traversal. 

### Example data setup

We also need some data to run a query against. I suggest using the example graph and data 
from [here](https://jsteemann.github.io/downloads/code/world-graph-setup.js).
Please download the code from the link and store it in the filesystem using a filename
of `world-graph-setup.js`. Then start the ArangoShell and run the code from the file:

```js
require("internal").load("/path/to/file/world-graph-setup.js");
```

This should have made available two collections `v` and `e`, which we can use as a graph.

### Invoking the counter function from AQL

All that's missing now is a query that invokes the custom counting function on the data.
Here it is:
 
```
LET params = { 
  visitor : "myfunctions::vertexCounter", 
  visitorReturnsResults : false 
}
FOR result IN TRAVERSAL(v, e, "v/world", "inbound", params) 
  RETURN result
```

You can run the above query from the **AQL editor** in the web interface. It should only
produce a single value indicating the number of nodes visited during the traversal.

### Remarks

Please note the following things:

* the AQL query will use two collections as in the example graph. They are named `v` and 
  `e`. The vertices are connected via inbound edges. The traversal will start at the
  node with id `v/world` as specified. To use other collections, a different start vertex
  or outbound connections, please adjust the `TRAVERSAL(...)` call.

* the visitor function name (`visitor` attribute of the configuration) in the example
  is `myfunctions::vertexCounter`. Adjust as required!

* the `visitorReturnsResults` attribute was set to `false`. This means that the return
  value of the custom visitor function will be ignored. Instead, it can modify the traversal
  result in place. The previous result is handed to the custom visitor as the second 
  function call parameter (`result`). On the first call, result will be an empty array as
  nothing was produced yet. In this case the function pushes a start value of 0 into the 
  result. Afterwards, and an all further invocations, the visitor will increment the 
  array value at position 0 by 1.

* the solution using the custom visitor will be much faster than when using the default 
  visitor to return all matching nodes and counting them later. The advantage of the
  custom visitor counter will be more measurable the more vertices are visited and the
  more data they contain. 
 
### Further uses
 
The above example is using the AQL function `TRAVERSAL()`, which requires a vertex and 
an edge collection to be specified. Custom visitors can also be used in the
`GRAPH_TRAVERSAL()` AQL function, which does not require named collections but a named 
graph. To make the above query work on a graph `WorldGraph` and use `GRAPH_TRAVERSAL()`
instead of `TRAVERSAL()`, change the query to:

```
LET params = { 
  visitor : "myfunctions::vertexCounter", 
  visitorReturnsResults : false 
}
FOR result IN GRAPH_TRAVERSAL("WorldGraph", "v/world", "inbound", params) 
  RETURN result
```

In order to produce not only a single result value but counting vertices by other criteria,
use an object to hold multiple counter values.

The following counter function (also named `myfunctions::vertexCounter`) counts the number
of vertices visited grouped by the value of their `type` attributes:

```js
var aqlfunctions = require("org/arangodb/aql/functions");

aqlfunctions.register("myfunctions::vertexCounter", function (config, result, vertex) {
  if (result.length === 0) {
    result.push({ });
  }
  var vertexType = vertex.type;
  if (! result[0].hasOwnProperty(vertexType)) {
    result[0][vertexType] = 1;
  }
  else {
    result[0][vertexType]++;
  }
});
```

You can use the same AQL query as above to run the query.

Custom visitors such as the counter function in this example can be combined with custom 
filter functions, too. This allows restricting results to only certain types of vertices, 
or following only specific edges/connections/paths in the graph.

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #graph #traversal #aql
