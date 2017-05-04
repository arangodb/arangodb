Smart Graph Management
======================

This chapter describes the JavaScript interface for [creating and modifying SmartGraphs](../README.md).
At first you have to note that every SmartGraph is a specialized version of a General Graph, which means all of the General Graph functionality is available on a SmartGraph as well.
The major difference of both modules is handling of the underlying collections, the General Graph does not enforce or maintain any sharding of the collections and can therefor combine arbitrary sets of existing collections.
SmartGraphs enforce and rely on a special sharding of the underlying collections and hence can only work with collections that are created through the SmartGraph itself.
This also means that SmartGraphs cannot be overlapping, a collection can either be sharded for one SmartGraph or for the other.
If you need to make sure that all queries can be executed with SmartGraph performance, just create one large SmartGraph covering everything and query it stating the subset of edge collections explicitly.
To generally understand the concept of this module please read the chapter about [General Graph Management](../GeneralGraphs/Management.md) first.
In the following we will only describe the overloaded functionality.
Everything else works identical in both modules.

Create a graph
--------------

Also SmartGraphs require edge relations to be created, the format of the relations is identical.
The only difference is that all collections used within the relations to create a new SmartGraph cannot exist yet. They have to be created by the Graph in order to enforce the correct sharding.



Create a graph

`graph_module._create(graphName, edgeDefinitions, orphanCollections, smartOptions)`

The creation of a graph requires the name and some SmartGraph options.
Due to the API `edgeDefinitions` and `orphanCollections` have to be given, but
both can be empty arrays and can be created later.
The `edgeDefinitions` can be created using the convenience method `_relation` known from the `general-graph` module, which is also available here.
`orphanCollections` again is just a list of additional vertex collections which are not yet connected via edges but should follow the same sharding to be connected later on.
All collections used within the creation process are newly created.
The process will fail if one of them already exists.
All newly created collections will immediately be dropped again in the failed case.

**Parameters**

* graphName (required) Unique identifier of the graph
* edgeDefinitions (required) List of relation definition objects, may be empty
* orphanCollections (required) List of additional vertex collection names, may be empty
* smartOptions (required) A JSON object having the following keys:
  * numberOfShards (required)
  The number of shards that will be created for each collection. To maintain the correct sharding all collections need an identical number of shards. This cannot be modified after creation of the graph.
  * smartGraphAttribute (required)
  The attribute that will be used for sharding. All vertices are required to have this attribute set and it has to be a string. Edges derive the attribute from their connected vertices.


**Examples**


Create an empty graph, edge definitions can be added at runtime:


    arangosh> var graph_module = require("@arangodb/smart-graph");
    arangosh> var graph = graph_module._create("myGraph", [], [], {smartGraphAttribute: "region", numberOfShards: 9});
    [ SmartGraph myGraph EdgeDefinitions: [ ] VertexCollections: [ ] ]


Create a graph using an edge collection `edges` and a single vertex collection `vertices` 


    arangosh> var graph_module = require("@arangodb/smart-graph");
    arangosh> var edgeDefinitions = [ graph_module._relation("edges", "vertices", "vertices") ];
    arangosh> var graph = graph_module._create("myGraph", edgeDefinitions, [], {smartGraphAttribute: "region", numberOfShards: 9});
    [ SmartGraph myGraph EdgeDefinitions: [ "edges: [vertices] -> [vertices]" ] VertexCollections: [ ] ]


Create a graph with edge definitions and orphan collections:


    arangosh> var graph_module = require("@arangodb/smart-graph");
    arangosh> var edgeDefinitions = [ graph_module._relation("myRelation", ["male", "female"], ["male", "female"]) ];
    arangosh> var graph = graph_module._create("myGraph", edgeDefinitions, ["sessions"], {smartGraphAttribute: "region", numberOfShards: 9});
    [ Graph myGraph EdgeDefinitions: [ 
      "myRelation: [female, male] -> [female, male]" 
    ] VertexCollections: [ 
      "sessions" 
    ] ]


Modify a graph definition during runtime
----------------------------------------

After you have created a SmartGraph its definition is also not immutable.
You can still add or remove relations.
This is again identical to General Graphs.
However there is one important difference:
You can only add collections that either *do not exist*, or that have been created by this graph earlier.
The later can be achieved if you for example remove an orphan collection from this graph, without dropping the collection itself.
Than after some time you decide to add it again, it can be used.
This is because the enforced sharding is still applied to this vertex collection, hence it is suitable to be added again.


#### Remove a vertex collection



Remove a vertex collection from the graph

`graph._removeVertexCollection(vertexCollectionName, dropCollection)`

In most cases this function works identically to the General Graph one.
But there is one special case:
The first vertex collection added to the graph (either orphan or within a relation) defines the sharding for all collections within the graph.
This collection can never be removed from the graph.


**Parameters**

* vertexCollectionName (required) Name of vertex collection.
* dropCollection (optional) If true the collection will be dropped if it is
  not used in any other graph. Default: false.

**Examples**

The following example shows that you cannot drop the initial collection.
You have to drop the complete graph.
If you just want to get rid of the data `truncate` it.


      arangosh> var graph_module = require("@arangodb/smart-graph")
      arangosh> var relation = graph_module._relation("edges", "vertices", "vertices");
      arangosh> var graph = graph_module._create("myGraph", [relation], ["other"], {smartGraphAttribute: "region", numberOfShards: 9});
      arangosh> graph._orphanCollections();
      [
        "other"
      ]
      arangosh> graph._deleteEdgeDefinition("edges");
      arangosh> graph._orphanCollections();
      [
        "vertices",
        "other"
      ]
      arangosh> graph._removeVertexCollection("other");
      arangosh> graph._orphanCollections();
      [
        "vertices"
      ]
      arangosh> graph._removeVertexCollection("vertices");
      ArangoError 4002: cannot drop this smart collection


