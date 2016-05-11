
@startDocuBlock JSF_general_graph_vertex_create_http_examples
@brief create a new vertex

@RESTHEADER{POST /_api/gharial/{graph-name}/vertex/{collection-name}, Create a vertex}

@RESTDESCRIPTION
Adds a vertex to the given collection.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTPARAM{collection-name, string, required} 
The name of the vertex collection the vertex belongs to.

@RESTQUERYPARAMETERS

@RESTPARAM{waitForSync, boolean, optional}
Define if the request should wait until synced to disk.

@RESTALLBODYPARAM{storeThisObject,object,required}
The body has to be the JSON object to be stored.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the vertex could be added and waitForSync is true.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTRETURNCODE{404}
Returned if no graph or no vertex collection with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialAddVertex}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/vertex/male";
  body = {
    name: "Francis"
  }
  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

