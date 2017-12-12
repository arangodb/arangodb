# Using a custom visitor from node.js

## Problem

I want to traverse a graph using a custom visitor from node.js.

## Solution

Use [arangojs](https://www.npmjs.com/package/arangojs) and an AQL query with a custom
visitor.

### Installing arangojs

First thing is to install *arangojs*.
This can be done using *npm* or *bower*:

```
npm install arangojs
```

or

```
bower install arangojs
```

### Example data setup

For the following example, we need the example graph and data from 
[here](https://jsteemann.github.io/downloads/code/world-graph-setup.js).
Please download the code from the link and store it in the filesystem using a filename
of `world-graph-setup.js`. Then start the ArangoShell and run the code from the file:

```js
require("internal").load("/path/to/file/world-graph-setup.js");
```

The script will create the following two collections and load some data into them:

- `v`: a collection with vertex documents
- `e`: an edge collection containing the connections between vertices in `v`

### Registering a custom visitor function

Let's register a custom visitor function now. A custom visitor function is a JavaScript
function that is executed every time the traversal processes a vertex in the graph.

To register a custom visitor function, we can execute the following commands in the 
ArangoShell:

```js
var aqlfunctions = require("org/arangodb/aql/functions");

aqlfunctions.register("myfunctions::leafNodeVisitor", function (config, result, vertex, path, connected) {
  if (connected && connected.length === 0) {
    return vertex.name + " (" + vertex.type + ")";
  }
});
```

### Invoking the custom visitor

The following code can be run in node.js to execute an AQL query that will
make use of the custom visitor:
 
```js
Database = require('arangojs'); 

/* connection the database, change as required */
db = new Database('http://127.0.0.1:8529'); 

/* the query string */
var query = "FOR result IN TRAVERSAL(v, e, @vertex, 'inbound', @options) RETURN result";

/* bind parameters */
var bindVars = { 
  vertex: "v/world",  /* our start vertex */
  options: {
    order: "preorder-expander",
    visitor: "myfunctions::leafNodeVisitor",
    visitorReturnsResults: true 
  }
};

db.query(query, bindVars, function (err, cursor) {
  if (err) {
    console.log('error: %j', err);
  } else {
    cursor.all(function(err2, list) {
      if (err) {
        console.log('error: %j', err2);
      } else {
        console.log("all document keys: %j", list);
      }
    });
  }
});
```

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #graph #traversal #aql #nodejs
