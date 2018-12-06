Graph Management
================

This chapter describes the javascript interface for [creating and modifying named graphs](../README.md).
In order to create a non empty graph the functionality to create edge definitions has to be introduced first:

Edge Definitions
----------------

An edge definition is always a directed relation of a graph. Each graph can have arbitrary many relations defined within the edge definitions array.

### Initialize the list



Create a list of edge definitions to construct a graph.

`graph_module._edgeDefinitions(relation1, relation2, ..., relationN)`

The list of edge definitions of a graph can be managed by the graph module itself.
This function is the entry point for the management and will return the correct list.


**Parameters**

* relationX (optional) An object representing a definition of one relation in the graph

**Examples**


    @startDocuBlockInline generalGraphEdgeDefinitionsSimple
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitionsSimple}
      var graph_module = require("@arangodb/general-graph");
      directed_relation = graph_module._relation("lives_in", "user", "city");
      undirected_relation = graph_module._relation("knows", "user", "user");
      edgedefinitions = graph_module._edgeDefinitions(directed_relation, undirected_relation);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphEdgeDefinitionsSimple



### Extend the list



Extend the list of edge definitions to construct a graph.

`graph_module._extendEdgeDefinitions(edgeDefinitions, relation1, relation2, ..., relationN)`

In order to add more edge definitions to the graph before creating
this function can be used to add more definitions to the initial list.


**Parameters**

* edgeDefinitions (required) A list of relation definition objects.
* relationX (required) An object representing a definition of one relation in the graph

**Examples**


    @startDocuBlockInline generalGraphEdgeDefinitionsExtend
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitionsExtend}
      var graph_module = require("@arangodb/general-graph");
      directed_relation = graph_module._relation("lives_in", "user", "city");
      undirected_relation = graph_module._relation("knows", "user", "user");
      edgedefinitions = graph_module._edgeDefinitions(directed_relation);
      edgedefinitions = graph_module._extendEdgeDefinitions(undirected_relation);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphEdgeDefinitionsExtend



### Relation

Define a directed relation.

`graph_module._relation(relationName, fromVertexCollections, toVertexCollections)`

The *relationName* defines the name of this relation and references to the underlying edge collection.
The *fromVertexCollections* is an Array of document collections holding the start vertices.
The *toVertexCollections* is an Array of document collections holding the target vertices.
Relations are only allowed in the direction from any collection in *fromVertexCollections*
to any collection in *toVertexCollections*.


**Parameters**

* relationName (required) The name of the edge collection where the edges should be stored.
  Will be created if it does not yet exist.
* fromVertexCollections (required) One or a list of collection names. Source vertices for the edges
  have to be stored in these collections. Collections will be created if they do not exist.
* toVertexCollections (required) One or a list of collection names. Target vertices for the edges
  have to be stored in these collections. Collections will be created if they do not exist.


**Examples**


    @startDocuBlockInline generalGraphRelationDefinitionSave
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRelationDefinitionSave}
      var graph_module = require("@arangodb/general-graph");
      graph_module._relation("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphRelationDefinitionSave

    @startDocuBlockInline generalGraphRelationDefinitionSingle
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRelationDefinitionSingle}
      var graph_module = require("@arangodb/general-graph");
      graph_module._relation("has_bought", "Customer", "Product");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphRelationDefinitionSingle


Create a graph
--------------

After having introduced edge definitions a graph can be created.



Create a graph

`graph_module._create(graphName, edgeDefinitions, orphanCollections)`

The creation of a graph requires the name of the graph and a definition of its edges.

For every type of edge definition a convenience method exists that can be used to create a graph.
Optionally a list of vertex collections can be added, which are not used in any edge definition.
These collections are referred to as orphan collections within this chapter.
All collections used within the creation process are created if they do not exist.


**Parameters**

* graphName (required) Unique identifier of the graph
* edgeDefinitions (optional) List of relation definition objects
* orphanCollections (optional) List of additional vertex collection names


**Examples**


Create an empty graph, edge definitions can be added at runtime:

    @startDocuBlockInline generalGraphCreateGraphNoData
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphNoData}
      var graph_module = require("@arangodb/general-graph");
      graph = graph_module._create("myGraph");
    ~ graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphCreateGraphNoData

Create a graph using an edge collection `edges` and a single vertex collection `vertices` 

    @startDocuBlockInline generalGraphCreateGraphSingle
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphSingle}
    ~ db._drop("edges");
    ~ db._drop("vertices");
      var graph_module = require("@arangodb/general-graph");
      var edgeDefinitions = [ { collection: "edges", "from": [ "vertices" ], "to" : [ "vertices" ] } ];
      graph = graph_module._create("myGraph", edgeDefinitions);
    ~ graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphCreateGraphSingle

Create a graph with edge definitions and orphan collections:

    @startDocuBlockInline generalGraphCreateGraph2
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraph2}
      var graph_module = require("@arangodb/general-graph");
    | graph = graph_module._create("myGraph",
      [graph_module._relation("myRelation", ["male", "female"], ["male", "female"])], ["sessions"]);
    ~ graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphCreateGraph2



### Complete Example to create a graph

Example Call:



    @startDocuBlockInline general_graph_create_graph_example1
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph_create_graph_example1}
      var graph_module = require("@arangodb/general-graph");
      var edgeDefinitions = graph_module._edgeDefinitions();
      graph_module._extendEdgeDefinitions(edgeDefinitions, graph_module._relation("friend_of", "Customer", "Customer"));
    | graph_module._extendEdgeDefinitions(
    | edgeDefinitions, graph_module._relation(
      "has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
      graph_module._create("myStore", edgeDefinitions);
    ~ graph_module._drop("myStore");
    ~ db._drop("Electronics");
    ~ db._drop("Customer");
    ~ db._drop("Groceries");
    ~ db._drop("Company");
    ~ db._drop("has_bought");
    ~ db._drop("friend_of");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph_create_graph_example1



alternative call:



    @startDocuBlockInline general_graph_create_graph_example2
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph_create_graph_example2}
      var graph_module = require("@arangodb/general-graph");
    |  var edgeDefinitions = graph_module._edgeDefinitions(
    |  graph_module._relation("friend_of", ["Customer"], ["Customer"]), graph_module._relation(
       "has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
      graph_module._create("myStore", edgeDefinitions);
    ~ graph_module._drop("myStore");
    ~ db._drop("Electronics");
    ~ db._drop("Customer");
    ~ db._drop("Groceries");
    ~ db._drop("Company");
    ~ db._drop("has_bought");
    ~ db._drop("friend_of");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph_create_graph_example2



List available graphs
---------------------



List all graphs.

`graph_module._list()`

Lists all graph names stored in this database.


**Examples**


    @startDocuBlockInline generalGraphList
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphList}
      var graph_module = require("@arangodb/general-graph");
      graph_module._list();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphList


Load a graph
------------



Get a graph

`graph_module._graph(graphName)`

A graph can be retrieved by its name.


**Parameters**

* graphName (required) Unique identifier of the graph


**Examples**


Get a graph:

    @startDocuBlockInline generalGraphLoadGraph
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphLoadGraph}
    ~ var examples = require("@arangodb/graph-examples/example-graph.js");
    ~ var g1 = examples.loadGraph("social");
      var graph_module = require("@arangodb/general-graph");
      graph = graph_module._graph("social");
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphLoadGraph



Remove a graph
--------------


Remove a graph

`graph_module._drop(graphName, dropCollections)`

A graph can be dropped by its name.
This can drop all collections contained in the graph as long as they are not used within other graphs.
To drop the collections only belonging to this graph, the optional parameter *drop-collections* has to be set to *true*.

**Parameters**

* graphName (required) Unique identifier of the graph
* dropCollections (optional) Define if collections should be dropped (default: false)

**Examples**


Drop a graph and keep collections:

    @startDocuBlockInline generalGraphDropGraphKeep
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphKeep}
    ~ var examples = require("@arangodb/graph-examples/example-graph.js");
    ~ var g1 = examples.loadGraph("social");
      var graph_module = require("@arangodb/general-graph");
      graph_module._drop("social");
      db._collection("female");
      db._collection("male");
      db._collection("relation");
    ~ db._drop("female");
    ~ db._drop("male");
    ~ db._drop("relation");
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphDropGraphKeep

    @startDocuBlockInline generalGraphDropGraphDropCollections
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphDropCollections}
    ~ var examples = require("@arangodb/graph-examples/example-graph.js");
    ~ var g1 = examples.loadGraph("social");
      var graph_module = require("@arangodb/general-graph");
      graph_module._drop("social", true);
      db._collection("female");
      db._collection("male");
      db._collection("relation");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphDropGraphDropCollections



Modify a graph definition at runtime
------------------------------------

After you have created an graph its definition is not immutable.
You can still add, delete or modify edge definitions and vertex collections.

### Extend the edge definitions



Add another edge definition to the graph

`graph._extendEdgeDefinitions(edgeDefinition)`

Extends the edge definitions of a graph. If an orphan collection is used in this
edge definition, it will be removed from the orphanage. If the edge collection of
the edge definition to add is already used in the graph or used in a different
graph with different *from* and/or *to* collections an error is thrown.


**Parameters**

* edgeDefinition (required) The relation definition to extend the graph


**Examples**


    @startDocuBlockInline general_graph__extendEdgeDefinitions
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph__extendEdgeDefinitions}
      var graph_module = require("@arangodb/general-graph")
    ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
      var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
      var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
      var graph = graph_module._create("myGraph", [ed1]);
      graph._extendEdgeDefinitions(ed2);
    ~ var blub = graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph__extendEdgeDefinitions



### Modify an edge definition



Modify an relation definition

`graph_module._editEdgeDefinitions(edgeDefinition)`

Edits one relation definition of a graph. The edge definition used as argument will
replace the existing edge definition of the graph which has the same collection.
Vertex Collections of the replaced edge definition that are not used in the new
definition will transform to an orphan. Orphans that are used in this new edge
definition will be deleted from the list of orphans. Other graphs with the same edge
definition will be modified, too.


**Parameters**

* edgeDefinition (required) The edge definition to replace the existing edge
  definition with the same attribute *collection*.


**Examples**


    @startDocuBlockInline general_graph__editEdgeDefinition
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph__editEdgeDefinition}
      var graph_module = require("@arangodb/general-graph")
    ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
      var original = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
      var modified = graph_module._relation("myEC1", ["myVC2"], ["myVC3"]);
      var graph = graph_module._create("myGraph", [original]);
      graph._editEdgeDefinitions(modified);
    ~ var blub = graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph__editEdgeDefinition



### Delete an edge definition



Delete one relation definition

`graph_module._deleteEdgeDefinition(edgeCollectionName, dropCollection)`

Deletes a relation definition defined by the edge collection of a graph. If the
collections defined in the edge definition (collection, from, to) are not used
in another edge definition of the graph, they will be moved to the orphanage.


**Parameters**

* edgeCollectionName (required) Name of edge collection in the relation definition.
* dropCollection (optional) Define if the edge collection should be dropped. Default false.

**Examples**


Remove an edge definition but keep the edge collection:

    @startDocuBlockInline general_graph__deleteEdgeDefinitionNoDrop
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinitionNoDrop}
      var graph_module = require("@arangodb/general-graph")
    ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
      var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
      var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
      var graph = graph_module._create("myGraph", [ed1, ed2]);
      graph._deleteEdgeDefinition("myEC1");
      db._collection("myEC1");
    ~ db._drop("myEC1");
    ~ var blub = graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph__deleteEdgeDefinitionNoDrop

Remove an edge definition and drop the edge collection:

    @startDocuBlockInline general_graph__deleteEdgeDefinitionWithDrop
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinitionWithDrop}
      var graph_module = require("@arangodb/general-graph")
    ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
      var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
      var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
      var graph = graph_module._create("myGraph", [ed1, ed2]);
      graph._deleteEdgeDefinition("myEC1", true);
      db._collection("myEC1");
    ~ db._drop("myEC1");
    ~ var blub = graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph__deleteEdgeDefinitionWithDrop



### Extend vertex Collections

Each graph can have an arbitrary amount of vertex collections, which are not part of any edge definition of the graph.
These collections are called orphan collections.
If the graph is extended with an edge definition using one of the orphans,
it will be removed from the set of orphan collection automatically.

#### Add a vertex collection



Add a vertex collection to the graph

`graph._addVertexCollection(vertexCollectionName, createCollection)`

Adds a vertex collection to the set of orphan collections of the graph. If the
collection does not exist, it will be created. If it is already used by any edge
definition of the graph, an error will be thrown.


**Parameters**

* vertexCollectionName (required) Name of vertex collection.
* createCollection (optional) If true the collection will be created if it does not exist. Default: true.

**Examples**


    @startDocuBlockInline general_graph__addVertexCollection
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph__addVertexCollection}
      var graph_module = require("@arangodb/general-graph");
    ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
      var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
      var graph = graph_module._create("myGraph", [ed1]);
      graph._addVertexCollection("myVC3", true);
    ~ db._drop("myVC3");
    ~ var blub = graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph__addVertexCollection



#### Get the orphaned collections



Get all orphan collections

`graph._orphanCollections()`

Returns all vertex collections of the graph that are not used in any edge definition.


**Examples**


    @startDocuBlockInline general_graph__orphanCollections
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph__orphanCollections}
      var graph_module = require("@arangodb/general-graph")
    ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
      var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
      var graph = graph_module._create("myGraph", [ed1]);
      graph._addVertexCollection("myVC3", true);
      graph._orphanCollections();
    ~ var blub = graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph__orphanCollections



#### Remove a vertex collection



Remove a vertex collection from the graph

`graph._removeVertexCollection(vertexCollectionName, dropCollection)`

Removes a vertex collection from the graph.
Only collections not used in any relation definition can be removed.
Optionally the collection can be deleted, if it is not used in any other graph.


**Parameters**

* vertexCollectionName (required) Name of vertex collection.
* dropCollection (optional) If true the collection will be dropped if it is
  not used in any other graph. Default: false.

**Examples**


    @startDocuBlockInline general_graph__removeVertexCollections
    @EXAMPLE_ARANGOSH_OUTPUT{general_graph__removeVertexCollections}
      var graph_module = require("@arangodb/general-graph")
    ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
      var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
      var graph = graph_module._create("myGraph", [ed1]);
      graph._addVertexCollection("myVC3", true);
      graph._addVertexCollection("myVC4", true);
      graph._orphanCollections();
      graph._removeVertexCollection("myVC3");
      graph._orphanCollections();
    ~ db._drop("myVC3");
    ~ var blub = graph_module._drop("myGraph", true);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock general_graph__removeVertexCollections




Manipulating Vertices
---------------------

### Save a vertex



Create a new vertex in vertexCollectionName

`graph.vertexCollectionName.save(data)`


**Parameters**

* data (required) JSON data of vertex.


**Examples**


    @startDocuBlockInline generalGraphVertexCollectionSave
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionSave}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.male.save({name: "Floyd", _key: "floyd"});
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphVertexCollectionSave



### Replace a vertex



Replaces the data of a vertex in collection vertexCollectionName

`graph.vertexCollectionName.replace(vertexId, data, options)`


**Parameters**

* vertexId (required) *_id* attribute of the vertex
* data (required) JSON data of vertex.
* options (optional) See [collection documentation](../../DataModeling/Documents/DocumentMethods.md)


**Examples**


    @startDocuBlockInline generalGraphVertexCollectionReplace
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionReplace}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.male.save({neym: "Jon", _key: "john"});
      graph.male.replace("male/john", {name: "John"});
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphVertexCollectionReplace



### Update a vertex



Updates the data of a vertex in collection vertexCollectionName

`graph.vertexCollectionName.update(vertexId, data, options)`


**Parameters**

* vertexId (required) *_id* attribute of the vertex
* data (required) JSON data of vertex.
* options (optional) See [collection documentation](../../DataModeling/Documents/DocumentMethods.md)

**Examples**


    @startDocuBlockInline generalGraphVertexCollectionUpdate
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionUpdate}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.female.save({name: "Lynda", _key: "linda"});
      graph.female.update("female/linda", {name: "Linda", _key: "linda"});
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphVertexCollectionUpdate



### Remove a vertex



Removes a vertex in collection *vertexCollectionName*

`graph.vertexCollectionName.remove(vertexId, options)`

Additionally removes all ingoing and outgoing edges of the vertex recursively
(see [edge remove](#remove-an-edge)).


**Parameters**

* vertexId (required) *_id* attribute of the vertex
* options (optional) See [collection documentation](../../DataModeling/Documents/DocumentMethods.md)


**Examples**


    @startDocuBlockInline generalGraphVertexCollectionRemove
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionRemove}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.male.save({name: "Kermit", _key: "kermit"});
      db._exists("male/kermit")
      graph.male.remove("male/kermit")
      db._exists("male/kermit")
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphVertexCollectionRemove



Manipulating Edges
------------------

### Save a new edge



Creates an edge from vertex *from* to vertex *to* in collection edgeCollectionName

`graph.edgeCollectionName.save(from, to, data, options)`


**Parameters**

* from (required) *_id* attribute of the source vertex
* to (required) *_id* attribute of the target vertex
* data (required) JSON data of the edge
* options (optional) See [collection documentation](../Edges/README.md)


**Examples**


    @startDocuBlockInline generalGraphEdgeCollectionSave1
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave1}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.relation.save("male/bob", "female/alice", {type: "married", _key: "bobAndAlice"});
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphEdgeCollectionSave1

If the collections of *from* and *to* are not defined in an edge definition of the graph,
the edge will not be stored.

    @startDocuBlockInline generalGraphEdgeCollectionSave2
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave2}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      | graph.relation.save(
      |  "relation/aliceAndBob",
      |   "female/alice",
         {type: "married", _key: "bobAndAlice"}); // xpError(ERROR_GRAPH_INVALID_EDGE)
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphEdgeCollectionSave2


### Replace an edge



Replaces the data of an edge in collection edgeCollectionName. Note that `_from` and `_to` are mandatory.

`graph.edgeCollectionName.replace(edgeId, data, options)`


**Parameters**

* edgeId (required) *_id* attribute of the edge
* data (required) JSON data of the edge
* options (optional) See [collection documentation](../../DataModeling/Documents/DocumentMethods.md)


**Examples**


    @startDocuBlockInline generalGraphEdgeCollectionReplace
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionReplace}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.relation.save("female/alice", "female/diana", {typo: "nose", _key: "aliceAndDiana"});
      graph.relation.replace("relation/aliceAndDiana", {type: "knows", _from: "female/alice", _to: "female/diana"});
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphEdgeCollectionReplace



### Update an edge



Updates the data of an edge in collection edgeCollectionName

`graph.edgeCollectionName.update(edgeId, data, options)`


**Parameters**

* edgeId (required) *_id* attribute of the edge
* data (required) JSON data of the edge
* options (optional) See [collection documentation](../../DataModeling/Documents/DocumentMethods.md)


**Examples**


    @startDocuBlockInline generalGraphEdgeCollectionUpdate
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionUpdate}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.relation.save("female/alice", "female/diana", {type: "knows", _key: "aliceAndDiana"});
      graph.relation.update("relation/aliceAndDiana", {type: "quarreled", _key: "aliceAndDiana"});
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphEdgeCollectionUpdate



### Remove an edge



Removes an edge in collection edgeCollectionName

`graph.edgeCollectionName.remove(edgeId, options)`

If this edge is used as a vertex by another edge, the other edge will be removed (recursively).


**Parameters**

* edgeId (required) *_id* attribute of the edge
* options (optional) See [collection documentation](../../DataModeling/Documents/DocumentMethods.md)


**Examples**


    @startDocuBlockInline generalGraphEdgeCollectionRemove
    @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionRemove}
      var examples = require("@arangodb/graph-examples/example-graph.js");
      var graph = examples.loadGraph("social");
      graph.relation.save("female/alice", "female/diana", {_key: "aliceAndDiana"});
      db._exists("relation/aliceAndDiana")
      graph.relation.remove("relation/aliceAndDiana")
      db._exists("relation/aliceAndDiana")
    ~ examples.dropGraph("social");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock generalGraphEdgeCollectionRemove
