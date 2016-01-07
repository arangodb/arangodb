////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_restrict
/// @brief Restricts the last statement in the chain to return
/// only elements of a specified set of collections
///
/// `graph_query.restrict(restrictions)`
///
/// By default all collections in the graph are searched for matching elements
/// whenever vertices and edges are requested.
/// Using *restrict* after such a statement allows to restrict the search
/// to a specific set of collections within the graph.
/// Restriction is only applied to this one part of the query.
/// It does not effect earlier or later statements.
///
/// @PARAMS
///
/// @PARAM{restrictions, array, optional}
/// Define either one or a list of collections in the graph.
/// Only elements from these collections are taken into account for the result.
///
/// @EXAMPLES
///
/// Request all directly connected vertices unrestricted:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnrestricted}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.edges().vertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Apply a restriction to the directly connected vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestricted}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.edges().vertices().restrict("female").toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Restriction of a query is only valid for collections known to the graph:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestrictedUnknown}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.edges().vertices().restrict(["female", "male", "products"]).toArray(); // xpError(ERROR_BAD_PARAMETER);
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////