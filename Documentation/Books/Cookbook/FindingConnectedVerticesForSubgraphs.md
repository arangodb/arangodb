# Search for vertices of special type connecting a given subgraph

## Acknowledgments

This problem has come up as a question on [Stackoverflow][1].
All credits for the problem go to [sergeda][2].
I liked the problem so much that I decided to write this recipe about it.

## Problem

Assume you have large graph consisting of two types of vertices: circles and crosses.
These are connected in an arbitrary way: circles to circles and circles to crosses.
Crosses to crosses is not allowed.
We have given a subset of the vertices (circles) and these tend to be connected via a set of intermediate vertices of type cross, however there might be multiple connecting crosses.

![Example dataset][3]

We want to identify these crosses and sort them by the number of links they have with the given subgraph.
As one example mapped to the dataset on the image we give the set of vertices A, B, C and D.
We expect to return E and F with E ordered before F as E connects more parts of the graph (4 vs. 3).

To map this problem on a real-world use case one could think of a package delivery service.
Here crosses are intermediate storage and circles are final destinations.
A connection is drawn whenever the final destination is reachable in a reasonable amount of time from the given intermediate storage.
The question now is to identify the best intermediate storage to hold the packages that should be delivered in one go on the next day.
Therefore we have to identify and rank all possible storages for a given subgraph (final destinations for the next day).

## Solution

Test Dataset creation in arangosh:

```js
var graph_module = require("org/arangodb/general-graph");
var graph = graph_module._create("myGraph", [
    graph_module._relation("edges", "circles", ["circles", "crosses"])]);

// Add circle circles
graph.circles.save({"_key": "A"});
graph.circles.save({"_key": "B"});
graph.circles.save({"_key": "C"});
graph.circles.save({"_key": "D"});
graph.circles.save({"_key": "G"});
graph.circles.save({"_key": "H"});

// Add crosses
graph.crosses.save({"_key": "E"});
graph.crosses.save({"_key": "F"});

// Add relevant edges
graph.edges.save("circles/A", "crosses/E", {});
graph.edges.save("circles/A", "crosses/F", {});
graph.edges.save("circles/B", "crosses/E", {});
graph.edges.save("circles/C", "crosses/E", {});
graph.edges.save("circles/D", "crosses/E", {});
graph.edges.save("circles/B", "crosses/F", {});
graph.edges.save("circles/C", "crosses/F", {});
graph.edges.save("circles/D", "crosses/F", {});

// Add irrelevant edges to make sure they are ignored
graph.edges.save("circles/G", "crosses/F", {});
graph.edges.save("circles/H", "crosses/F", {});
```

Now we have created the graph as described in the problem scenario above.
Now we are given a set of vertices within the graph: `["circles/A","circles/B","circles/C","circles/D"]`
For this recipe we hard-code this exact subset, in production we would replace it by a bindParameter to a result of another subquery.
Many of ArangoDBs graph functions accept a predefined set of vertex documents as input parameters so this is already a good start.
Next up we have to identify "connecting" vertices.
A vertex is connecting two vertices "A" and "B" if it is a direct neighbor of "A" and a direct neighbor of "B" in the definition of our problem.
This allows us to use the built-in AQL function `GRAPH_COMMON_NEIGHBORS` that identifies all of these candidates.
We can simply drop in the set of vertices for both comparison sets in this function.
This will form all distinct vertex pairs in our graph and list all neighbors they have in common.
This leads us to an intermediate result (taking only A and all its distinct pairs):

```json
[
  {
    "nodes/A": {
      "nodes/B": [
        {
          "_id": "crosses/E",
          "_rev": "220352236897",
          "_key": "E"
        }
      ],
        "nodes/C": [
        {
          "_id": "crosses/E",
          "_rev": "220352236897",
          "_key": "E"
        }
      ],
        "nodes/D": [
        {
          "_id": "crosses/E",
          "_rev": "220352236897",
          "_key": "E"
        }
      ]
    }
  }
]
```

Now we are only interested in the "_key" value of the inner most element.
We can extract this value by iterating over all internal VALUES two times:

    FOR f IN VALUES(n)
      FOR s IN VALUES(f)
        FOR candidate IN s 
        RETURN s._key

This will give us a list of all common neighbors, but we only want to count them using collect.

    COLLECT crosses = candidate._key INTO counter

This will now count for each connecting node how many pairs it connects.
These pairs are considered distinct based on their ordering (A and B is considered a different pair as B and A).
With combinatoric knowledge we know that `n * (n-1) = counter` is the formula to identify the real amount of vertices n forming connections.
This can be transformed into an expression we can execute in AQL `0.5 + SQRT(0.25 + LENGTH(counter))` following the Reduced quadratic equation.

Now we can identify all connecting vertices for a given subset of the graph.
However we are only interested in the ones of type cross.
As we have used a different collection for them we can simply apply a restriction to `GRAPH_COMMON_NEIGHBORS` to only consider vertices from this special collection.
The last thing to do is to put everything together:

    FOR x IN (
      (
         LET circles = ["circles/A", "circles/B", "circles/C", "circles/D"]
         LET condition = {"vertexCollectionRestriction": "crosses"}
         FOR n IN GRAPH_COMMON_NEIGHBORS("myGraph", circles, circles, condition, condition)
           FOR f IN VALUES(n)
             FOR s IN VALUES(f)
               FOR candidate IN s 
                 COLLECT crosses = candidate._key INTO counter
                 RETURN {
                   crosses: crosses,
                   connections: 0.5 + SQRT(0.25 + LENGTH(counter))
                 }
        )
      )
    SORT x.connections DESC
    RETURN x

Which will yield this result on the dataset in the image:

```json
[
  {
    "crosses": "E",
    "connections": 4
  },
  {
    "crosses": "F",
    "connections": 3
  }
]
```

## Comment

**Author:** [Michael Hackstein](https://github.com/mchacki)

**Tags:** #graph

[1]: http://stackoverflow.com/questions/27520753/find-the-cross-node-for-number-of-nodes-in-arangodb/27530898?noredirect=1#comment43506796_27530898
[2]: http://stackoverflow.com/users/2592822/sergeda
[3]: ./assets/FindingConnectedVerticesForSubgraphs/example_graph.png
