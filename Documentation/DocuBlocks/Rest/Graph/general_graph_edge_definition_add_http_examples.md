@startDocuBlock general_graph_edge_definition_add_http_examples
@brief Add a new edge definition to the graph

@RESTHEADER{POST /_api/gharial/{graph}/edge, Add edge definition}

@RESTDESCRIPTION
Adds an additional edge definition to the graph.

This edge definition has to contain a *collection* and an array of
each *from* and *to* vertex collections.  An edge definition can only
be added if this definition is either not used in any other graph, or
it is used with exactly the same definition. It is not possible to
store a definition "e" from "v1" to "v2" in the one graph, and "e"
from "v2" to "v1" in the other graph.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTBODYPARAM{collection,string,required,string}
The name of the edge collection to be used.

@RESTBODYPARAM{from,array,required,string}
One or many vertex collections that can contain source vertices.

@RESTBODYPARAM{to,array,required,string}
One or many vertex collections that can contain target vertices.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the definition could be added successfully and
waitForSync is enabled for the `_graphs` collection.
The response body contains the graph configuration that has been stored.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the modified graph.

@RESTRETURNCODE{202}
Returned if the definition could be added successfully and
waitForSync is disabled for the `_graphs` collection.
The response body contains the graph configuration that has been stored.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the modified graph.

@RESTRETURNCODE{400}
Returned if the definition could not be added.
This could be because it is ill-formed, or
if the definition is used in an other graph with a different signature.

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
In order to modify a graph you at least need to have the following privileges:<br>
  1. `Administrate` access on the Database.

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
Returned if no graph with this name could be found.

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

@EXAMPLE_ARANGOSH_RUN{HttpGharialAddEdgeCol}
  var examples = require("@arangodb/graph-examples/example-graph.js");
~ examples.dropGraph("social");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/edge";
  body = {
    collection: "works_in",
    from: ["female", "male"],
    to: ["city"]
  };
  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
