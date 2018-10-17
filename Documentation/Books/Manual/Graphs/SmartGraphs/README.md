SmartGraphs
===========

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

This chapter describes the [smart-graph](../README.md) module.
It enables you to manage graphs at scale, it will give a vast performance benefit for all graphs sharded in an ArangoDB Cluster.
On a single server this feature is pointless, hence it is only available in a cluster mode.
In terms of querying there is no difference between smart and General Graphs.
The former are a transparent replacement for the latter.
So for querying the graph please refer to [AQL Graph Operations](../../../AQL/Graphs/index.html) 
and [Graph Functions](../GeneralGraphs/Functions.md) sections.
The optimizer is clever enough to identify if we are on a SmartGraph or not.

The difference is only in the management section: creating and modifying the underlying collections of the graph.
For a detailed API reference please refer to [SmartGraph Management](../SmartGraphs/Management.md).

Do the hands-on
[ArangoDB SmartGraphs Tutorial](https://www.arangodb.com/using-smartgraphs-arangodb/)
to learn more.

What makes a graph smart?
-------------------------

Most graphs have one feature that divides the entire graph into several smaller subgraphs.
These subgraphs have a large amount of edges that only connect vertices in the same subgraph
and only have few edges connecting vertices from other subgraphs.
Examples for these graphs are:

* Social Networks

  Typically the feature here is the region/country users live in.
  Every user typically has more contacts in the same region/country then she has in other regions/countries

* Transport Systems

  For those also the feature is the region/country. You have many local transportation but only few across countries.

* E-Commerce

  In this case probably the category of products is a good feature. Often products of the same category are bought together.

If this feature is known, SmartGraphs can make use if it.
When creating a SmartGraph you have to define a smartAttribute, which is the name of an attribute stored in every vertex.
The graph will than be automatically sharded in such a way that all vertices with the same value are stored on the same physical machine,
all edges connecting vertices with identical smartAttribute values are stored on this machine as well.
During query time the query optimizer and the query executor both know for every document exactly where it is stored and can thereby minimize network overhead.
Everything that can be computed locally will be computed locally.

Benefits of SmartGraphs
-----------------------

Because of the above described guaranteed sharding, the performance of queries that only cover one subgraph have a performance almost equal to an only local computation.
Queries that cover more than one subgraph require some network overhead. The more subgraphs are touched the more network cost will apply.
However the overall performance is never worse than the same query on a General Graph.

Getting started
---------------

First of all SmartGraphs *cannot use existing collections*, when switching to SmartGraph from an existing data set you have to import the data into a fresh SmartGraph.
This switch can be easily achieved with [arangodump](../../Programs/Arangodump/README.md)
and [arangorestore](../../Programs/Arangorestore/README.md).
The only thing you have to change in this pipeline is that you create the new collections with the SmartGraph before starting `arangorestore`.

* Create a graph

  In comparison to General Graph we have to add more options when creating the graph. The two options `smartGraphAttribute` and `numberOfShards` are required and cannot be modified later. 


    @startDocuBlockInline smartGraphCreateGraphHowTo1
      arangosh> var graph_module = require("@arangodb/smart-graph");
      arangosh> var graph = graph_module._create("myGraph", [], [], {smartGraphAttribute: "region", numberOfShards: 9});
      arangosh> graph;
      [ SmartGraph myGraph EdgeDefinitions: [ ] VertexCollections: [ ] ]
    @endDocuBlock smartGraphCreateGraphHowTo1


* Add some vertex collections

  This is again identical to General Graph. The module will setup correct sharding for all these collections. *Note*: The collections have to be new.


    @startDocuBlockInline smartGraphCreateGraphHowTo2
      arangosh> graph._addVertexCollection("shop");
      arangosh> graph._addVertexCollection("customer");
      arangosh> graph._addVertexCollection("pet");
      arangosh> graph;
      [ SmartGraph myGraph EdgeDefinitions: [ ] VertexCollections: [ "shop", "customer", "pet" ] ]
    @endDocuBlock smartGraphCreateGraphHowTo2


* Define relations on the Graph


    @startDocuBlockInline smartGraphCreateGraphHowTo3
      arangosh> var rel = graph_module._relation("isCustomer", ["shop"], ["customer"]);
      arangosh> graph._extendEdgeDefinitions(rel);
      arangosh> graph;
      [ SmartGraph myGraph EdgeDefinitions: [   "isCustomer: [shop] -> [customer]" ] VertexCollections: [ "pet" ] ]
    @endDocuBlock smartGraphCreateGraphHowTo3
