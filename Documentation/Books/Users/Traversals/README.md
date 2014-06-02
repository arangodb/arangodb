<a name="traversals"></a>
# Traversals

ArangoDB provides several ways to query graph data. Very simple operations can
be composed with the low-level edge methods `edges`, `inEdges`, and `outEdges` for
[edge collections](../Edges/Edges.md). For more complex operations,
ArangoDB provides predefined traversal objects.

For any of the following examples, we'll be using the example collections `v` and `e`,
populated with continents, countries and capitals data listed below (see [Example Data](../Traversals/ExampleData.md)).
