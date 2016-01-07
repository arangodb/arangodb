////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_drop
/// @brief Remove a graph
///
/// `graph_module._drop(graphName, dropCollections)`
///
/// A graph can be dropped by its name.
/// This will automatically drop all collections contained in the graph as
/// long as they are not used within other graphs.
/// To drop the collections, the optional parameter *drop-collections* can be set to *true*.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @PARAM{dropCollections, boolean, optional}
/// Define if collections should be dropped (default: false)
///
/// @EXAMPLES
///
/// Drop a graph and keep collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphKeep}
/// ~ var examples = require("@arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("@arangodb/general-graph");
///   graph_module._drop("social");
///   db._collection("female");
///   db._collection("male");
///   db._collection("relation");
/// ~ db._drop("female");
/// ~ db._drop("male");
/// ~ db._drop("relation");
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphDropCollections}
/// ~ var examples = require("@arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("@arangodb/general-graph");
///   graph_module._drop("social", true);
///   db._collection("female");
///   db._collection("male");
///   db._collection("relation");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////