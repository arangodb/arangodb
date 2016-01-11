////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_count
/// @brief Returns the number of returned elements if the query is executed.
///
/// `graph_query.count()`
///
/// This function determines the amount of elements to be expected within the result of the query.
/// It can be used at the beginning of execution of the query
/// before using *next()* or in between *next()* calls.
/// The query object maintains a cursor of the query for you.
/// *count()* does not change the cursor position.
///
/// @EXAMPLES
///
/// To count the number of matched elements:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLCount}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices();
///   query.count();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////