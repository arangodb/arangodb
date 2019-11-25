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
Must contain a revision.
If this is set a document is only returned if
it has exactly this revision.
Also see if-match header as an alternative to this.

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

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{vertex,object,required,vertex_representation}
The complete vertex.

@RESTRETURNCODE{304}
Returned if the if-none-match header is given and the
currently stored vertex still has this revision value.
So there was no update between the last time the vertex
was fetched by the caller.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to update vertices in the graph  you at least need to have the following privileges:<br>
  1. `Read Only` access on the Database.
  2. `Read Only` access on the given collection.

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
* The vertex does not exist.

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
