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
Flag if there was an error (true) or not (false)
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation_3}
The information about the newly created graph.

@RESTSTRUCT{name,graph_representation_3,string,required,}
The name of the graph.

@RESTSTRUCT{edgeDefinitions,graph_representation_3,array,required,post_api_gharial_create_edge_defs}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation_3,array,required,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation_3,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{replicationFactor,graph_representation_3,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation_3,string,required,}
The internal id value of this graph. 

@RESTSTRUCT{_rev,graph_representation_3,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation_3,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{isSmart,graph_representation_3,boolean,required,}
Flag if the graph is a SmartGraph (Enterprise only) or not.

@RESTSTRUCT{smartGraphAttribute,graph_representation_3,string,optional,}
The name of the sharding attribute in smart graph case (Enterprise Only)

@RESTRETURNCODE{202}
Is returned if the collection could be created and waitForSync is disabled
for the `_graphs` collection, or given in the request.
The response body contains the graph configuration that has been stored.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false)
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code

@RESTREPLYBODY{graph,object,required,graph_representation_4}
The information about the newly created graph

@RESTSTRUCT{name,graph_representation_4,string,required,}
The name of the graph

@RESTSTRUCT{edgeDefinitions,graph_representation_4,array,required,post_api_gharial_create_edge_defs}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation_4,array,required,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation_4,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{replicationFactor,graph_representation_4,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation_4,string,required,}
The internal id value of this graph. 

@RESTSTRUCT{_rev,graph_representation_4,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation_4,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{isSmart,graph_representation_4,boolean,required,}
Flag if the graph is a SmartGraph (Enterprise only) or not.

@RESTSTRUCT{smartGraphAttribute,graph_representation_4,string,optional,}
The name of the sharding attribute in smart graph case (Enterprise Only)

@RESTRETURNCODE{400}
Returned if the request is in an invalid format.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occured.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to modify a graph you at least need to have the following privileges:

  1. `Administrate` access on the Database.
  2. `Read Only` access on every collection used within this graph.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occured.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false)
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occured.

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
