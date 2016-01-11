////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_neighbors
/// @brief Select all neighbors of the vertices selected in the step before.
///
/// `graph_query.neighbors(examples, options)`
///
/// Creates an AQL statement to select all neighbors for each of the vertices selected
/// in the step before.
/// The resulting set of vertices can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition-of-examples)
///
/// @PARAM{options, object, optional}
///   An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition-of-examples)
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *vertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///   * *minDepth*: Defines the minimal number of intermediate steps to neighbors (default is 1).
///   * *maxDepth*: Defines the maximal number of intermediate steps to neighbors (default is 1).
///
/// @EXAMPLES
///
/// To request unfiltered neighbors:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsUnfiltered}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.neighbors().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered neighbors by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredSingle}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.neighbors({name: "Bob"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered neighbors by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredMultiple}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.vertices([{name: "Bob"}, {name: "Charly"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////