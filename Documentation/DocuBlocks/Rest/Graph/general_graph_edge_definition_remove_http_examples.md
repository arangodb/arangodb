@startDocuBlock general_graph_edge_definition_remove_http_examples
@brief Remove an edge definition form the graph

@RESTHEADER{DELETE /_api/gharial/{graph}/edge/{definition}#definition, Remove an edge definition from the graph}

@RESTDESCRIPTION
Remove one edge definition from the graph.  This will only remove the
edge collection, the vertex collections remain untouched and can still
be used in your queries.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTURLPARAM{definition,string,required}
The name of the edge collection used in the definition.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Define if the request should wait until synced to disk.

@RESTQUERYPARAM{dropCollections,boolean,optional}
Drop the collection as well.
Collection will only be dropped if it is not used in other graphs.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the edge definition could be removed from the graph
and waitForSync is true.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the modified graph.

@RESTRETURNCODE{202}
Returned if the edge definition could be removed from the graph and
waitForSync is false.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the modified graph.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to drop a vertex you at least need to have the following privileges:
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
Returned if no graph with this name could be found,
or if no edge definition with this name is found in the graph.

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

@EXAMPLE_ARANGOSH_RUN{HttpGharialEdgeDefinitionRemove}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/edge/relation";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);

  logJsonResponse(response);
  db._drop("relation");
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
