
@startDocuBlock get_api_gharial
@brief Lists all graphs known to the graph module.

@RESTHEADER{GET /_api/gharial, List all graphs, listGraphs}

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
A list of all named graphs.

@RESTSTRUCT{graph,graph_list,object,optional,graph_representation}
The properties of the named graph.

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
