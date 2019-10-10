---
layout: default
description: There are multiple ways to work with graphs in ArangoDB,as well as different ways to query your graphs using AQL
---
Graphs in AQL
=============

There are multiple ways to work with [graphs in ArangoDB](../graphs.html),
as well as different ways to query your graphs using AQL.

The two options in managing graphs are to either use

- named graphs where ArangoDB manages the collections involved in one graph, or
- graph functions on a combination of document and edge collections.

Named graphs can be defined through the [graph-module](../graphs-general-graphs.html)
or via the [web interface](../programs-web-interface.html).
The definition contains the name of the graph, and the vertex and edge collections
involved. Since the management functions are layered on top of simple sets of
document and edge collections, you can also use regular AQL functions to work with them. 

Both variants (named graphs and loosely coupled collection sets a.k.a. anonymous graphs)
are supported by the AQL language constructs for graph querying. These constructs
make full use of optimizations and therefore best performance is to be expected:

- [AQL Traversals](graphs-traversals.html) to follow edges connected to a start vertex,
  up to a variable depth. It can be combined with AQL filter conditions.

- [AQL Shortest Path](graphs-shortest-path.html) to find the vertices and edges between two
  given vertices, with as few hops as possible.

These types of queries are only useful if you use edge collections and/or graphs in
your data model.

{% hint 'info' %}
New to graphs? [**Take our free graph course for freshers**](https://www.arangodb.com/arangodb-graph-course/){:target="_blank"}
and get from zero knowledge to advanced query techniques.
{% endhint %}
