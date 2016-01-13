////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_drop_http_examples
/// @brief delete an existing graph
/// 
/// @RESTHEADER{DELETE /_api/gharial/{graph-name}, Drop a graph}
/// 
/// @RESTDESCRIPTION
/// Removes a graph from the collection *\_graphs*.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{dropCollections, boolean, optional}
/// Drop collections of this graph as well.
/// Collections will only be dropped if they are not used in other graphs.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the graph could be dropped.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialDrop}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social?dropCollections=true";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////