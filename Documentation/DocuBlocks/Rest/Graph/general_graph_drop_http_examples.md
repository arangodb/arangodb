@startDocuBlock general_graph_drop_http_examples
@brief delete an existing graph

@RESTHEADER{DELETE /_api/gharial/{graph}, Drop a graph}

@RESTDESCRIPTION
Drops an existing graph object by name.
Optionally all collections not used by other graphs
can be dropped as well.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{dropCollections,boolean,optional}
Drop collections of this graph as well.  Collections will only be
dropped if they are not used in other graphs.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Is returned if the graph could be dropped and waitForSync is enabled
for the `_graphs` collection, or given in the request.

@RESTRETURNCODE{202}
Is returned if the graph could be dropped and waitForSync is disabled
for the `_graphs` collection and not given in the request.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to drop a graph you at least need to have the following privileges:
  1. `Administrate` access on the Database.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

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

@EXAMPLE_ARANGOSH_RUN{HttpGharialDrop}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var url = "/_api/gharial/social?dropCollections=true";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);

  logJsonResponse(response);
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
