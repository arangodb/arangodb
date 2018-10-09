@startDocuBlock general_graph_vertex_get_http_examples
@brief fetches an existing vertex

@RESTHEADER{GET /_api/gharial/{graph}/vertex/{collection}/{vertex}, Get a vertex}

@RESTDESCRIPTION
Gets a vertex from the given collection.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTURLPARAM{collection,string,required} 
The name of the vertex collection the vertex belongs to.

@RESTURLPARAM{vertex,string,required} 
The *_key* attribute of the vertex.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{rev,string,optional}
Must contain a revision 

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{if-match,string,optional}
If the "If-Match" header is given, then it must contain exactly one Etag. The document is returned,
if it has the same revision as the given Etag. Otherwise a HTTP 412 is returned. As an alternative
you can supply the Etag in an query parameter *rev*.

@RESTHEADERPARAM{if-none-match,string,optional}
If the "If-None-Match" header is given, then it must contain exactly one Etag. The document is returned,
only if it has a different revision as the given Etag. Otherwise a HTTP 304 is returned. 

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the vertex could be found.

@RESTRETURNCODE{404}
Returned if no graph with this name, no vertex collection or no vertex with this id could be found.

@RESTRETURNCODE{412}
Returned if if-match header is given, but the documents revision is different.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialGetVertex}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/vertex/female/alice";
  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
