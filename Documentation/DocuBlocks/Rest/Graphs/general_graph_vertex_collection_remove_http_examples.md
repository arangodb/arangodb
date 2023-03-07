@startDocuBlock general_graph_vertex_collection_remove_http_examples
@brief Remove a vertex collection form the graph.

@RESTHEADER{DELETE /_api/gharial/{graph}/vertex/{collection}, Remove vertex collection}

@RESTDESCRIPTION
Removes a vertex collection from the graph and optionally deletes the collection,
if it is not used in any other graph.
It can only remove vertex collections that are no longer part of edge definitions,
if they are used in edge definitions you are required to modify those first.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTURLPARAM{collection,string,required}
The name of the vertex collection.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{dropCollection,boolean,optional}
Drop the collection as well.
Collection will only be dropped if it is not used in other graphs.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the vertex collection was removed from the graph successfully
and waitForSync is true.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the newly created graph

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the newly created graph

@RESTRETURNCODE{400}
Returned if the vertex collection is still used in an edge definition.
In this case it cannot be removed from the graph yet, it has to be
removed from the edge definition first.

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

You can remove vertex collections that are not used in any edge collection:

@EXAMPLE_ARANGOSH_RUN{HttpGharialRemoveVertexCollection}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  var g = examples.loadGraph("social");
  g._addVertexCollection("otherVertices");
  var url = "/_api/gharial/social/vertex/otherVertices";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);

  logJsonResponse(response);
~ examples.dropGraph("social");
~ db._drop("otherVertices");
@END_EXAMPLE_ARANGOSH_RUN

You cannot remove vertex collections that are used in edge collections:

@EXAMPLE_ARANGOSH_RUN{HttpGharialRemoveVertexCollectionFailed}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  var g = examples.loadGraph("social");
  var url = "/_api/gharial/social/vertex/male";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 400);

  logJsonResponse(response);
  db._drop("male");
  db._drop("female");
  db._drop("relation");
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
