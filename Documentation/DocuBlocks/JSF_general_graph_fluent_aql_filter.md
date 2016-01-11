////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_filter
/// @brief Filter the result of the query
///
/// `graph_query.filter(examples)`
///
/// This can be used to further specfiy the expected result of the query.
/// The result set is reduced to the set of elements that matches the given *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition-of-examples)
///
/// @EXAMPLES
///
/// Request vertices unfiltered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnfilteredVertices}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Request vertices filtered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFilteredVertices}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().filter({name: "Alice"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Request edges unfiltered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnfilteredEdges}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().outEdges().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Request edges filtered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFilteredEdges}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().outEdges().filter({type: "married"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////