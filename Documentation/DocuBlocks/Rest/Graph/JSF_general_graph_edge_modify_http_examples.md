
@startDocuBlock JSF_general_graph_edge_modify_http_examples
@brief modify an existing edge

@RESTHEADER{PATCH /_api/gharial/{graph-name}/edge/{collection-name}/{edge-key}, Modify an edge}

@RESTDESCRIPTION
Updates the data of the specific edge in the collection.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTPARAM{collection-name, string, required}
The name of the edge collection the edge belongs to.

@RESTPARAM{edge-key, string, required}
The *_key* attribute of the vertex.

@RESTQUERYPARAMETERS

@RESTPARAM{waitForSync, boolean, optional}
Define if the request should wait until synced to disk.

@RESTPARAM{keepNull, boolean, optional}
Define if values set to null should be stored. By default the key is not removed from the document.

@RESTALLBODYPARAM{updateAttributes,object,required}
The body has to be a JSON object containing the attributes to be updated.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the edge could be updated.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTRETURNCODE{404}
Returned if no graph with this name, no edge collection or no edge with this id could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialPatchEdge}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var any = require("@arangodb").db.relation.any();
  var url = "/_api/gharial/social/edge/relation/" + any._key;
  body = {
    since: "01.01.2001"
  }
  var response = logCurlRequest('PATCH', url, body);
  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

