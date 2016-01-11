

@brief Request the next element in the result.

`graph_query.next()`

The generated statement maintains a cursor for you.
If this cursor is already present *next()* will
use this cursors position to deliver the next result.
Also the cursor position will be moved by one.
If the query has not yet been executed *next()*
will execute it and create the cursor for you.
It will throw an error of your query has no further results.

@EXAMPLES

Request some elements with next:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNext}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices();
  query.next();
  query.next();
  query.next();
  query.next();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

The cursor is recreated if the query is changed:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNextRecreate}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices();
  query.next();
  query.edges();
  query.next();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

