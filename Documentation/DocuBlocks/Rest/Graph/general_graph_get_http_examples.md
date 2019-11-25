
@startDocuBlock general_graph_get_http_examples
@brief Get a graph from the graph module.

@RESTHEADER{GET /_api/gharial/{graph}, Get a graph}

@RESTDESCRIPTION
Selects information for a given graph.
Will return the edge definitions as well as the orphan collections.
Or returns a 404 if the graph does not exist.

@RESTURLPARAMETERS

@RESTURLPARAM{graph,string,required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returns the graph if it could be found.
The result will have the following format:

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the newly created graph

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

@EXAMPLE_ARANGOSH_RUN{HttpGharialGetGraph}
  var graph = require("@arangodb/general-graph");
| if (graph._exists("myGraph")) {
|    graph._drop("myGraph", true);
  }
  graph._create("myGraph", [{
    collection: "edges",
    from: [ "startVertices" ],
    to: [ "endVertices" ]
  }]);
  var url = "/_api/gharial/myGraph";

  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);

  graph._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
