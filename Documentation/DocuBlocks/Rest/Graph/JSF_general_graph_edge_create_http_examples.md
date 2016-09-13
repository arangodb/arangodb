
@startDocuBlock JSF_general_graph_edge_create_http_examples
@brief Creates an edge in an existing graph

@RESTHEADER{POST /_api/gharial/{graph-name}/edge/{collection-name}, Create an edge}

@RESTDESCRIPTION
Creates a new edge in the collection.
Within the body the has to contain a *_from* and *_to* value referencing to valid vertices in the graph.
Furthermore the edge has to be valid in the definition of this
[edge collection](../../Manual/Appendix/Glossary.html#edge-collection).

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTPARAM{collection-name, string, required} 
The name of the edge collection the edge belongs to.

@RESTQUERYPARAMETERS

@RESTPARAM{waitForSync, boolean, optional}
Define if the request should wait until synced to disk.

@RESTPARAM{_from, string, required}

@RESTPARAM{_to, string, required}

@RESTALLBODYPARAM{storeThisJsonObject,object,required}
The body has to be the JSON object to be stored.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the edge could be created.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTRETURNCODE{404}
Returned if no graph with this name, no edge collection or no edge with this id could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialAddEdge}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/edge/relation";
  body = {
    type: "friend",
    _from: "female/alice",
    _to: "female/diana"
  };
  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

