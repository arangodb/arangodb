
@startDocuBlock general_graph_edge_get_http_examples
@brief fetch an edge

@RESTHEADER{GET /_api/gharial/{graph-name}/edge/{collection-name}/{edge-key}, Get an edge}

@RESTDESCRIPTION
Gets an edge from the given collection.

@RESTURLPARAMETERS

@RESTURLPARAM{graph-name,string,required}
The name of the graph.

@RESTURLPARAM{collection-name,string,required} 
The name of the edge collection the edge belongs to.

@RESTURLPARAM{edge-key,string,required} 
The *_key* attribute of the vertex.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{if-match,string,optional}
If the "If-Match" header is given, then it must contain exactly one Etag. The document is returned,
if it has the same revision as the given Etag. Otherwise a HTTP 412 is returned. As an alternative
you can supply the Etag in an attribute rev in the URL.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the edge could be found.

@RESTRETURNCODE{404}
Returned if no graph with this name, no edge collection or no edge with this id could be found.

@RESTRETURNCODE{412}
Returned if if-match header is given, but the documents revision is different.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialGetEdge}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var any = require("@arangodb").db.relation.any();
  var url = "/_api/gharial/social/edge/relation/" + any._key;
  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

