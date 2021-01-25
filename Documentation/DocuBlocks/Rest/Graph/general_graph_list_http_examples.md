
@startDocuBlock general_graph_list_http_examples
@brief Lists all graphs known to the graph module.

@RESTHEADER{GET /_api/gharial, List all graphs}

@RESTDESCRIPTION
Lists all graphs stored in this database.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the module is available and the graphs could be listed.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graphs,array,required,graph_list}

@RESTSTRUCT{graph,graph_list,object,optional,graph_representation}
The information about the newly created graph

@RESTSTRUCT{name,graph_representation,string,required,}
The name of the graph

@RESTSTRUCT{edgeDefinitions,graph_representation,array,required,graph_edge_definition}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation,array,required,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation,string,required,}
The internal id value of this graph.

@RESTSTRUCT{_rev,graph_representation,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation,integer,required,}
The replication factor used for every new collection in the graph.
Can also be the string `"satellite"` for a SmartGraph (Enterprise Edition only).

@RESTSTRUCT{isSmart,graph_representation,boolean,required,}
Whether the graph is a SmartGraph (Enterprise Edition only).

@RESTSTRUCT{isDisjoint,graph_representation,boolean,required,}
Whether the graph is a Disjoint SmartGraph (Enterprise Edition only).

@RESTSTRUCT{smartGraphAttribute,graph_representation,string,optional,}
Name of the sharding attribute in the SmartGraph case (Enterprise Edition only).

@RESTSTRUCT{isSatellite,graph_representation,boolean,required,}
Flag if the graph is a SatelliteGraph (Enterprise Edition only) or not.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialList}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  examples.loadGraph("routeplanner");
  var url = "/_api/gharial";
  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
~ examples.dropGraph("social");
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
