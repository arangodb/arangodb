Graphs in AQL
=============

As you already [read about graphs in ArangoDB](../Graphs/README.md) you can have several views on graphs.
There are also several ways to work with graphs in AQL.
You can use named graphs where ArangoDB manages the collections involved in one graph.
You can also use graph functions on a combination of document and edge collections.
named graphs are defined though the [graph-module](../GeneralGraphs/README.md), that contains the name of the graph, and the vertex and edge collections involved.
Since the management functions are layered on top of simple sets of document and edge collections, you can also use regular AQL functions to work with them. 

In AQL you can reach several graphing functions:
* [AQL Traversals](GraphTraversals.md) is making full use of optimisations and therefore best performance is to be expected. It can work on named graphs and loosely coupled collection sets (aka anonymous graphs). You can use AQL filter conditions on traversals.
* [Named graph Operations](GraphOperations.md) work on named graphs; offer a versatile range of parameters.
* [Other graph functions](GraphFunctions.md) work on single edge collection (which may also be part of named graphs).


