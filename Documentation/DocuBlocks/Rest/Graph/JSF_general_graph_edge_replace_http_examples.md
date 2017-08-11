@startDocuBlock JSF_general_graph_edge_replace_http_examples
@brief replace the content of an existing edge

@RESTHEADER{PUT /_api/gharial/{graph-name}/edge/{collection-name}/{edge-key}, Replace an edge}

@RESTDESCRIPTION
Replaces the data of an edge in the collection.

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

@RESTHEADERPARAMETERS

@RESTPARAM{if-match, string, optional}
If the "If-Match" header is given, then it must contain exactly one etag. The document is updated,
if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
you can supply the etag in an attribute rev in the URL.

@RESTALLBODYPARAM{storeThisJsonObject,object,required}
The body has to be the JSON object to be stored.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the request was successful but waitForSync is true.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTRETURNCODE{404}
Returned if no graph with this name, no edge collection or no edge with this id could be found.

@RESTRETURNCODE{412}
Returned if if-match header is given, but the documents revision is different.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialPutEdge}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var any = require("@arangodb").db.relation.any();
  var url = "/_api/gharial/social/edge/relation/" + any._key;
  body = {
    type: "divorced",
    _from: "female/alice",
    _to: "male/bob"
  }
  var response = logCurlRequest('PUT', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
