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
Flag if there was an error (true) or not (false)
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code

@RESTREPLYBODY{graph,object,required,graph_representation_6}
The information about the newly created graph

@RESTSTRUCT{name,graph_representation_6,string,required,}
The name of the graph

@RESTSTRUCT{edgeDefinitions,graph_representation_6,array,required,post_api_gharial_create_edge_defs}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation_6,array,required,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation_6,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{replicationFactor,graph_representation_6,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation_6,string,required,}
The internal id value of this graph. 

@RESTSTRUCT{_rev,graph_representation_6,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation_6,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{isSmart,graph_representation_6,boolean,required,}
Flag if the graph is a SmartGraph (Enterprise only) or not.

@RESTSTRUCT{smartGraphAttribute,graph_representation_6,string,optional,}
The name of the sharding attribute in smart graph case (Enterprise Only)

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false)
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code

@RESTREPLYBODY{graph,object,required,graph_representation_7}
The information about the newly created graph

@RESTSTRUCT{name,graph_representation_7,string,required,}
The name of the graph

@RESTSTRUCT{edgeDefinitions,graph_representation_7,array,required,post_api_gharial_create_edge_defs}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation_7,array,required,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation_7,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{replicationFactor,graph_representation_7,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation_7,string,required,}
The internal id value of this graph. 

@RESTSTRUCT{_rev,graph_representation_7,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation_7,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{isSmart,graph_representation_7,boolean,required,}
Flag if the graph is a SmartGraph (Enterprise only) or not.

@RESTSTRUCT{smartGraphAttribute,graph_representation_7,string,optional,}
The name of the sharding attribute in smart graph case (Enterprise Only)

@RESTRETURNCODE{400}
Returned if the vertex collection is still used in an edge definition.
In this case it cannot be removed from the graph yet, it has to be
removed from the edge definition first.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false)
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occured.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to drop a vertex you at least need to have the following privileges:
  1. `Administrate` access on the Database.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false)
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

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
