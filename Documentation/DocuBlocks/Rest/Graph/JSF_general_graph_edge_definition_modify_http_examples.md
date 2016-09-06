@startDocuBlock JSF_general_graph_edge_definition_modify_http_examples
@brief Replace an existing edge definition

@RESTHEADER{PUT /_api/gharial/{graph-name}/edge/{definition-name}, Replace an edge definition}

@RESTDESCRIPTION
Change one specific edge definition.
This will modify all occurrences of this definition in all graphs known to your database.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTPARAM{definition-name, string, required}
The name of the edge collection used in the definition.

@RESTBODYPARAM{collection,string,required,string}
The name of the edge collection to be used.

@RESTBODYPARAM{from,array,required,string}
One or many vertex collections that can contain source vertices.

@RESTBODYPARAM{to,array,required,string}
One or many edge collections that can contain target vertices.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the request was successful and waitForSync is true.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTRETURNCODE{400}
Returned if no edge definition with this name is found in the graph.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialReplaceEdgeCol}
  var examples = require("@arangodb/graph-examples/example-graph.js");
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
