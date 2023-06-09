@startDocuBlock put_api_gharial_graph_edge_definition

@RESTHEADER{PUT /_api/gharial/{graph}/edge/{collection}, Replace an edge definition, replaceEdgeDefinition}

@RESTDESCRIPTION
Change one specific edge definition.
This will modify all occurrences of this definition in all graphs known to your database.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTURLPARAM{collection,string,required}
The name of the edge collection used in the edge definition.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Define if the request should wait until synced to disk.

@RESTQUERYPARAM{dropCollections,boolean,optional}
Drop the collection as well.
Collection will only be dropped if it is not used in other graphs.

@RESTBODYPARAM{collection,string,required,string}
The name of the edge collection to be used.

@RESTBODYPARAM{from,array,required,string}
One or many vertex collections that can contain source vertices.

@RESTBODYPARAM{to,array,required,string}
One or many vertex collections that can contain target vertices.

@RESTBODYPARAM{options,object,optional,post_api_edgedef_modify_opts}
A JSON object to set options for modifying collections within this
edge definition.

@RESTSTRUCT{satellites,post_api_edgedef_modify_opts,array,optional,string}
An array of collection names that is used to create SatelliteCollections
for a (Disjoint) SmartGraph using SatelliteCollections (Enterprise Edition only).
Each array element must be a string and a valid collection name.
The collection type cannot be modified later.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the request was successful and waitForSync is true.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the modified graph.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the modified graph.

@RESTRETURNCODE{400}
Returned if the new edge definition is ill-formed and cannot be used.

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
Returned if no graph with this name could be found, or if no edge definition
with this name is found in the graph.

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

@EXAMPLE_ARANGOSH_RUN{HttpGharialReplaceEdgeCol}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/edge/relation";
  body = {
    collection: "relation",
    from: ["female", "male", "animal"],
    to: ["female", "male", "animal"]
  };
  var response = logCurlRequest('PUT', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
