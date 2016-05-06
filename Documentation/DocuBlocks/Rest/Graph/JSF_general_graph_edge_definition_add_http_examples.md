@startDocuBlock JSF_general_graph_edge_definition_add_http_examples
@brief Add a new edge definition to the graph

@RESTHEADER{POST /_api/gharial/{graph-name}/edge, Add edge definition}

@RESTDESCRIPTION
Adds an additional edge definition to the graph.

This edge definition has to contain a *collection* and an array of
each *from* and *to* vertex collections.  An edge definition can only
be added if this definition is either not used in any other graph, or
it is used with exactly the same definition. It is not possible to
store a definition "e" from "v1" to "v2" in the one graph, and "e"
from "v2" to "v1" in the other graph.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTBODYPARAM{collection,string,required,string}
The name of the edge collection to be used.

@RESTBODYPARAM{from,array,required,string}
One or many vertex collections that can contain source vertices.

@RESTBODYPARAM{to,array,required,string}
One or many edge collections that can contain target vertices.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the definition could be added successfully and
waitForSync is enabled for the `_graphs` collection.

@RESTRETURNCODE{202}
Returned if the definition could be added successfully and
waitForSync is disabled for the `_graphs` collection.

@RESTRETURNCODE{400}
Returned if the defininition could not be added, the edge collection
is used in an other graph with a different signature.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialAddEdgeCol}
  var examples = require("@arangodb/graph-examples/example-graph.js");
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
