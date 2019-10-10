---
layout: default
description: This chapter describes the general-graph module
---
Graphs
======

This chapter describes the [general-graph](graphs.html) module.
It allows you to define a graph that is spread across several edge and document collections.
This allows you to structure your models in line with your domain and group them logically in collections giving you the power to query them in the same graph queries.
There is no need to include the referenced collections within the query, this module will handle it for you.

New to ArangoDB? Take the free
[ArangoDB Graph Course](https://www.arangodb.com/arangodb-graph-course){:target="_blank"}
for freshers.

Three Steps to create a graph
-----------------------------

* Create a graph
{% arangoshexample examplevar="examplevar" script="script" result="result" %}
    @startDocuBlockInline generalGraphCreateGraphHowTo1
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo1}
      var graph_module = require("@arangodb/general-graph");
      var graph = graph_module._create("myGraph");
      graph;
    ~ graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphCreateGraphHowTo1
{% endarangoshexample %}
{% include arangoshexample.html id=examplevar script=script result=result %}
* Add some vertex collections
{% arangoshexample examplevar="examplevar" script="script" result="result" %}
    @startDocuBlockInline generalGraphCreateGraphHowTo2
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo2}
    ~ var graph_module = require("@arangodb/general-graph");
    ~ var graph = graph_module._create("myGraph");
      graph._addVertexCollection("shop");
      graph._addVertexCollection("customer");
      graph._addVertexCollection("pet");
      graph;
    ~ graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphCreateGraphHowTo2
{% endarangoshexample %}
{% include arangoshexample.html id=examplevar script=script result=result %}
* Define relations on the Graph
{% arangoshexample examplevar="examplevar" script="script" result="result" %}
    @startDocuBlockInline generalGraphCreateGraphHowTo3
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo3}
    ~ var graph_module = require("@arangodb/general-graph");
    ~ var graph = graph_module._create("myGraph");
    ~ graph._addVertexCollection("pet");
      var rel = graph_module._relation("isCustomer", ["shop"], ["customer"]);
      graph._extendEdgeDefinitions(rel);
      graph;
    ~ graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphCreateGraphHowTo3
{% endarangoshexample %}
{% include arangoshexample.html id=examplevar script=script result=result %}

