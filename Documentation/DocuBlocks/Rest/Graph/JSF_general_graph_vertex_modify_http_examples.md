
@startDocuBlock JSF_general_graph_vertex_modify_http_examples
@brief replace an existing vertex

@RESTHEADER{PATCH /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}, Modify a vertex}

@RESTDESCRIPTION
Updates the data of the specific vertex in the collection.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTPARAM{collection-name, string, required} 
The name of the vertex collection the vertex belongs to.

@RESTPARAM{vertex-key, string, required} 
The *_key* attribute of the vertex.

@RESTQUERYPARAMETERS

@RESTPARAM{waitForSync, boolean, optional}
Define if the request should wait until synced to disk.

@RESTPARAM{keepNull, boolean, optional}
Define if values set to null should be stored. By default the key is not removed from the document.

@RESTHEADERPARAMETERS

@RESTPARAM{if-match, string, optional}
If the "If-Match" header is given, then it must contain exactly one etag. The document is updated,
if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
you can supply the etag in an attribute rev in the URL.

@RESTALLBODYPARAM{replaceAttributes,object,required}
The body has to contain a JSON object containing exactly the attributes that should be replaced.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the vertex could be updated.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTRETURNCODE{404}
Returned if no graph with this name, no vertex collection or no vertex with this id could be found.

@RESTRETURNCODE{412}
Returned if if-match header is given, but the documents revision is different.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialModifyVertex}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  body = {
    age: 26
  }
  var url = "/_api/gharial/social/vertex/female/alice";
  var response = logCurlRequest('PATCH', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

