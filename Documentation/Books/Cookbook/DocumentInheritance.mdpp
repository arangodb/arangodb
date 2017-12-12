# Model document inheritance

## Problem

How do you model document inheritance given that collections do not support that feature? 

## Solution

Lets assume you have three document collections: "subclass", "class" and "superclass". You also have two edge collections: "sub_extends_class" and "class_extends_super".

You can create them via arangosh or foxx:

```js
var graph_module = require("com/arangodb/general-graph");
var g = graph_module._create("inheritance");
g._extendEdgeDefinitions(graph_module. _directedRelation("sub_extends_class", ["subclass"], ["class"]));
g._extendEdgeDefinitions(graph_module. _directedRelation("class_extends_super", ["class"], ["superclass"]));
```

This makes sure when using the graph interface that the inheritance looks like:

* sub -> class
* class -> super
* super -> sub

To make sure everything works as expected you should use the built-in traversal in combination with Foxx. This allows you to add the inheritance security layer easily.
To use traversals in foxx simply add the following line before defining routes:

```js
var traversal = require("org/arangodb/graph/traversal");
var Traverser = traversal.Traverser;
```

Also you can add the following endpoint in Foxx:

```js
var readerConfig = {
  datasource: traversal.graphDatasourceFactory("inheritance"),
  expander: traversal.outboundExpander, // Go upwards in the tree
  visitor: function (config, result, vertex, path) {
    for (key in vertex) {
      if (vertex.hasOwnProperty(key) && !result.hasOwnProperty(key)) {
        result[key] = vertex[key] // Store only attributes that have not yet been found
      }
    }
  }
};  

controller.get("load/:collection/:key", function(req, res) {
  var result = {};
  var id = res.params("collection") + "/" + res.params("key");
  var traverser = new Traverser(readerConfig);
  traverser.traverse(result, g.getVertex(id));
  res.json(result);
});
```

This will make sure to iterate the complete inheritance tree upwards to the root element and will return all values on the path
were the first instance of this value is kept

## Comment
You should go with edges because it is much easier to query them if you have a theoretically unlimited depth in inheritance. 
If you have a fixed inheritance depth you could also go with an attribute in the document referencing the parent and execute joins in AQL.


**Author:** [Michael Hackstein](https://github.com/mchacki)

**Tags:** #graph #document