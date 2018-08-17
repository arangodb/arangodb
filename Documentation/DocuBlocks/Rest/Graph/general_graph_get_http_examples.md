
@startDocuBlock general_graph_get_http_examples
@brief Get a graph from the graph module.

@RESTHEADER{GET /_api/gharial/{graph}, Get a graph}

@RESTDESCRIPTION
Selects information for a given graph.
Will return the edge definitions as well as the orphan collections.
Or returns a 404 if the graph does not exist.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returns the graph if it could be found.
The result will have the following format:

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false)
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code

@RESTREPLYBODY{graph,object,required,graph_representation_5}
The information about the newly created graph

@RESTSTRUCT{name,graph_representation_5,string,required,}
The name of the graph

@RESTSTRUCT{edgeDefinitions,graph_representation_5,array,required,post_api_gharial_create_edge_defs}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation_5,array,required,post_api_gharial_create_orphans}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation_5,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{replicationFactor,graph_representation_5,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation_5,string,required,}
The internal id value of this graph. 

@RESTSTRUCT{_rev,graph_representation_5,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation_5,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{isSmart,graph_representation_5,boolean,required,}
Flag if the graph is a SmartGraph (Enterprise only) or not.

@RESTSTRUCT{smartGraphAttribute,graph_representation_5,string,optional,}
The name of the sharding attribute in smart graph case (Enterprise Only)

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

@EXAMPLE_ARANGOSH_RUN{HttpGharialGetGraph}
  var graph = require("@arangodb/general-graph");
| if (graph._exists("myGraph")) {
|    graph._drop("myGraph", true);
  }
  graph._create("myGraph", [{
    collection: "edges",
    from: [ "startVertices" ],
    to: [ "endVertices" ]
  }]);
  var url = "/_api/gharial/myGraph";

  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);

  graph._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

