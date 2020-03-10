@startDocuBlock general_graph_edge_delete_http_examples
@brief removes an edge from graph

@RESTHEADER{DELETE /_api/gharial/{graph}/edge/{collection}/{edge}, Remove an edge}

@RESTDESCRIPTION
Removes an edge from the collection.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTURLPARAM{collection,string,required}
The name of the edge collection the edge belongs to.

@RESTURLPARAM{edge,string,required}
The *_key* attribute of the edge.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
Define if the request should wait until synced to disk.

@RESTQUERYPARAM{returnOld,boolean,optional}
Define if a presentation of the deleted document should
be returned within the response object.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{if-match,string,optional}
If the "If-Match" header is given, then it must contain exactly one Etag. The document is updated,
if it has the same revision as the given Etag. Otherwise a HTTP 412 is returned. As an alternative
you can supply the Etag in an attribute rev in the URL.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the edge could be removed.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{removed,boolean,required,}
Is set to true if the remove was successful.

@RESTREPLYBODY{old,object,optional,edge_representation}
The complete deleted edge document.
Includes all attributes stored before this operation.
Will only be present if returnOld is true.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{removed,boolean,required,}
Is set to true if the remove was successful.

@RESTREPLYBODY{old,object,optional,edge_representation}
The complete deleted edge document.
Includes all attributes stored before this operation.
Will only be present if returnOld is true.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to delete vertices in the graph  you at least need to have the following privileges:<br>
  1. `Read Only` access on the Database.
  2. `Write` access on the given collection.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{404}
Returned in the following cases:
* No graph with this name could be found.
* This collection is not part of the graph.
* The edge to remove does not exist.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{412}
Returned if if-match header is given, but the stored documents revision is different.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialDeleteEdge}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var any = require("@arangodb").db.relation.any();
  var url = "/_api/gharial/social/edge/relation/" + any._key;
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
