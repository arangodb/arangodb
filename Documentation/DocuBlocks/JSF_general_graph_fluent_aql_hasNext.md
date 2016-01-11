

@brief Checks if the query has further results.

`graph_query.hasNext()`

The generated statement maintains a cursor for you.
If this cursor is already present *hasNext()* will
use this cursors position to determine if there are
further results available.
If the query has not yet been executed *hasNext()*
will execute it and create the cursor for you.

@EXAMPLES

Start query execution with hasNext:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLHasNext}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices();
  query.hasNext();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

Iterate over the result as long as it has more elements:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLHasNextIteration}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices();
| while (query.hasNext()) {
|   var entry = query.next();
|   // Do something with the entry
  }
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

