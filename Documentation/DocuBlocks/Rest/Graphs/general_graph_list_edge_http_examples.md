
@startDocuBlock general_graph_list_edge_http_examples
@brief Lists all edge definitions

@RESTHEADER{GET /_api/gharial/{graph}/edge, List edge definitions}

@RESTDESCRIPTION
Lists all edge collections within this graph.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the edge definitions could be listed.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{collections,array,required,string}
The list of all vertex collections within this graph.
Includes collections in edgeDefinitions as well as orphans.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialListEdge}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/edge";
  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
