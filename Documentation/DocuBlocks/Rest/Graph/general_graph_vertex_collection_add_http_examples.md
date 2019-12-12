@startDocuBlock general_graph_vertex_collection_add_http_examples
@brief Add an additional vertex collection to the graph.

@RESTHEADER{POST /_api/gharial/{graph}/vertex, Add vertex collection}

@RESTDESCRIPTION
Adds a vertex collection to the set of orphan collections of the graph.
If the collection does not exist, it will be created.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Is returned if the collection could be created and waitForSync is enabled
for the `_graphs` collection, or given in the request.
The response body contains the graph configuration that has been stored.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the modified graph.

@RESTRETURNCODE{202}
Is returned if the collection could be created and waitForSync is disabled
for the `_graphs` collection, or given in the request.
The response body contains the graph configuration that has been stored.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the newly created graph

@RESTRETURNCODE{400}
Returned if the request is in an invalid format.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to modify a graph you at least need to have the following privileges:<br>
  1. `Administrate` access on the Database.
  2. `Read Only` access on every collection used within this graph.

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

@EXAMPLE_ARANGOSH_RUN{HttpGharialAddVertexCol}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/vertex";
  body = {
    collection: "otherVertices"
  };
  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
