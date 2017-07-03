Distributed Iterative Graph Processing (Pregel)
======

Distributed graph processing enables you to do online analytical processing
directly on graphs stored into arangodb. This is intended to help you gain analytical insights
on your data, without having to use external processing sytems. Examples of algorithms
to execute are PageRank, Vertex Centrality, Vertex Closeness, Connected Components, Community Detection.
This system is not useful for typical online queries, where you just do work on a small set of vertices.
These kind of tasks are better suited for AQL.

The processing system inside ArangoDB is based on: [Pregel: A System for Large-Scale Graph Processing](http://www.dcs.bbk.ac.uk/~dell/teaching/cc/paper/sigmod10/p135-malewicz.pdf) – Malewicz et al. (Google) 2010
This concept enables us to perform distributed graph processing, without the need for distributed global locking.

# Prerequisites

If you are running a single ArangoDB instance in single-server mode, there are no requirements regarding the modeling 
of your data. All you need is at least one vertex collection and one edge collection. Note that the performance may be
better, if the number of your shards / collections matches the number of CPU cores.

When you use ArangoDB Community edition in cluster mode, you might need to model your collections in a certain way to
ensure correct results. For more information see the next section.

## Requirements for Collections in a Cluster (Non Smart Graph)

To enable iterative graph processing for your data, you will need to ensure
that your vertex and edge collections are sharded in a specific way.

The pregel computing model requires all edges to be present on the DB Server where
the vertex document identified by the `_from` value is located.
This means the vertex collections need to be sharded by '_key' and the edge collection
will need to be sharded after an attribute which always contains the '_key' of the vertex.

Our implementation currently requires every edge collection to be sharded after a "vertex" attributes,
additionally you will need to specify the key `distributeShardsLike` and an **equal** number of shards on every collection.
Only if these requirements are met can ArangoDB place the edges and vertices correctly.

For example you might create your collections like this:
```javascript
// Create main vertex collection: 
db._create("vertices", {
    shardKeys:['_key'],
    numberOfShards: 8
  });

// Optionally create arbitrary additional vertex collections 
db._create("additonal", {
    distributeShardsLike:"vertices", 
    numberOfShards:8
  });

// Create (one or more) edge-collections: 
db._createEdgeCollection("edges", {
    shardKeys:['vertex'],
    distributeShardsLike:"vertices",
    numberOfShards:8
  });
```

You will need to ensure that edge documents contain the proper values in their sharding attribute.
For a vertex document with the following content ```{_key:"A", value:0}```
the corresponding edge documents would have look like this:

```
  {_from:"vertices/A", _to: "vertices/B", vertex:"A"}
  {_from:"vertices/A", _to: "vertices/C", vertex:"A"}
  {_from:"vertices/A", _to: "vertices/D", vertex:"A"}
  ...
```

This will ensure that outgoing edge documents will be placed on the same DBServer as the vertex.
Without the correct placement of the edges, the pregel graph processing system will not work correctly, because
edges will not load correctly.

### Arangosh API

#### Starting an Algorithm Execution

The pregel API is accessible through the `@arangodb/pregel` package.
To start an execution you need to specify the **algorithm** name and the vertex and edge collections.
Alternatively you can specify a named graph. Additionally you can specify custom parameters which
vary for each algorithm.
The `start` method will always a unique ID which can be used to interact with the algorithm and later on.

The below version of the `start` method can be used for named graphs:
```javascript
  var pregel = require("@arangodb/pregel");
  var params = {};
  var execution = pregel.start("<algorithm>", "<yourgraph>", params);
```
Params needs to be an object, the valid keys are mentioned below in the section [Algorithms]()


Alternatively you might want to specify the vertex and edge collections directly. The call-syntax of the `start``
method changes in this case. The second argument must be an object with the keys `vertexCollections`and `edgeCollections`.
```javascript
  var pregel = require("@arangodb/pregel");
  var params = {};
  var execution = pregel.start("<algorithm>", {vertexCollections:["vertices"], edgeCollections:["edges"]}, {});
```
The last argument is still the parameter object. See below for a list of algorithms and parameters.


#### Status of an Algorithm Execution

The code returned by the `pregel.start(...)` method can be used to
track the status of your algorithm.

```javascript
  var execution = pregel.start("sssp", "demograph", {source: "vertices/V"});
  var status = pregel.status(execution);
```

The result will tell you the current status of the algorithm execution. 
It will tell you the current `state` of the execution, the current global superstep, the runtime, the global aggregator values as
well as the number of send and received messages.

Valid values for the `state` field include:
- "running" algorithm is still running
- "done": The execution is done, the result might not be written back into the collection yet.
- "canceled": The execution was permanently canceled, either by the user or by an error.
- "in error": The exeuction is in an error state. This can be caused by primary DBServers being not reachable or being non responsive.
  The execution might recover later, or switch to "canceled" if it was not able to recover successfuly
- "recovering": The execution is actively recovering, will switch back to "running" if the recovery was successful

The object returned by the `status` method might for example look something like this:

```javascript
  {
    "state" : "running",
    "gss" : 12,
    "totalRuntime" : 123.23,
    "aggregators" : {
      "converged" : false,
      "max" : true,
      "phase" : 2
    },
    "sendCount" : 3240364978,
    "receivedCount" : 3240364975
  }
```


#### Canceling an Execution / Discarding results

To cancel an execution which is still runnning, and discard any intermediare results you can use the `cancel` method.
This will immediatly free all memory taken up by the execution, and will make you lose all intermediary data. 

You might get inconsistent results if you cancel an execution while it is already in it's `done` state. The data is written
multi-threaded into all collection shards at once, this means there are multiple transactions simultaniously. A transaction might
already be commited when you cancel the execution job, therefore you might see the result in your collection. This does not apply
if you configured the execution to not write data into the collection.

```javascript
// start a single source shortest path job
var execution = pregel.start("sssp", "demograph", {source: "vertices/V"});
pregel.cancel(execution);
```

## AQL integration

ArangoDB supports retrieving temporary pregel results through the ArangoDB query language (AQL). 
When our graph processing subsystem finishes executing an algorithm, the result can either be written back into the 
database or kept in memory. In both cases the result can be queried via AQL. If the data was not written to the database
store it is only held temporarily, until the user calls the `cancel` methodFor example a user might want to query
only nodes with the most rank from the result set of a PageRank execution. 

    FOR v IN PREGEL_RESULT(<handle>)
      FILTER v.value >= 0.01
      RETURN v._key

## Available Algorithms

There are a number of general parameters which apply to almost all algorithms:
* `store`: Is per default *true*, the pregel engine will write results back to the database.
  if the value is *false* then you can query the results via AQLk, see AQL integration.
* `maxGSS`: Maximum number of global iterations for this algorithm
* `parallelism`: Number of parellel threads to use per worker. Does not influence the number of threads used to load
                 or store data from the database (this depends on the number of shards).
* `async`: Algorithms wich support async mode, will run without synchronized global iterations,
  might lead to performance increases if you have load imbalances.
* `resultField`: Most algorithms will write the result into this field

#### Page Rank

PageRank is a well known algorithm to rank documents in a graph. The algorithm will run until
the execution converges. Specify a custom threshold with the parameter `threshold`, to run for a fixed
number of iterations use the `maxGSS` parameter. 

```javascript
var pregel = require("@arangodb/pregel");
pregel.start("pagerank", "graphname", {maxGSS: 100, threshold:0.00000001})
```

#### Single-Source Shortest Path

Calculates the distance of each vertex to a certain shortest path. The algorithm will run until it converges,
the iterations are bound by the diameter (the longest shortest path) of your graph.

```javascript
  var pregel = require("@arangodb/pregel");
  pregel.start("sssp", "graphname", {source:"vertices/1337"})
```


#### Connected Components

There are two algorithms to find connected components in a graph. To find weakly connected components (WCC)
you can use the algorithm named "connectedcomponents", to find strongly connected components (SCC) you can use the algorithm
named "scc". Both algorithm will assign a component ID to each vertex.

A weakly connected components means that there exist a path from every vertex pair in that component.
WCC is a very simple and fast algorithm, which will only work correctly on undirected graphs. Your results on directed graphs may vary, depending on how connected your components are.

In the case of SCC a component means every vertex is reachable from any other vertex in the same component. The algorithm is more complex than the WCC algorithm and requires more RAM, because each vertex needs to store much more state. 
Consider using WCC if you think your data may be suitable for it.

```javascript
  var pregel = require("@arangodb/pregel");
  // weakly connected components
  pregel.start("connectedcomponents", "graphname")
  // strongly connected components
  pregel.start("scc", "graphname")
```

#### Hyperlink-Induced Topic Search (HITS)

HITS is a link analysis algorithm that rates Web pages, developed by Jon Kleinberg (The algorithm is also known as hubs and authorities).


The idea behind Hubs and Authorities comes from the typical structure of the web: Certain websites known as hubs, serve as large directories that are not actually 
authoritative on the information that they hold. These hubs are used as compilations of a broad catalog of information that leads users direct to other authoritative webpages.
The algorithm assigns each vertex two scores: The authority-score and the hub-score. The authority score rates how many good hubs point to a particular
vertex (or webpage), the hub score rates how good (authoritative) the vertices pointed to are. For more see https://en.wikipedia.org/wiki/HITS_algorithm

Our version of the algorithm
converges after a certain amount of time. The parameter *threshold* can be used to set a limit for the convergence (measured as maximum absolute difference of the hub and 
authority scores between the current and last iteration)
When you specify the result field name, the hub score will be stored in "<result field>_hub" and the authority score in 
"<result field>_auth".
The algorithm can be executed like this:


```javascript
    var pregel = require("@arangodb/pregel");
    var handle = pregel.start("hits", "yourgraph", {threshold:0.00001, resultField: "score"});
```

#### Vertex Centrality

Centrality measures help identify the most important vertices in a graph. They can be used in a wide range of applications:
For example they can be used to identify *influencers* in social networks, or *middle-men* in terrorist networks.
There are various definitions for centrality, the simplest one being the vertex degree. 
These definitions were not designed with scalability in mind. It is probably impossible to discover an efficient algorithm which computes them in a distributed way. 
Fortunately there are scalable substitutions available, which should be equally usable for most use cases.


![Illustration of an execution of different centrality measures (Freeman 1977)](centrality_visual.png)


##### Effective Closeness

A common definitions of centrality is the **closeness centrality** (or closeness). 
The closeness of a vertex in a graph is the inverse average length of the shortest path between the vertex 
and all other vertices. For vertices *x*, *y* and shortest distance *d(y,x)* it is defined as

![Vertex Closeness](closeness.png)

Effective Closeness approximates the closeness measure. The algorithm works by iteratively estimating the number
of shortest paths passing through each vertex. The score will approximates the the real closeness score, since
it is not possible to actually count all shortest paths due to the horrendous O(n^2 * d) memory requirements. 
The algorithm is from the paper *Centralities in Large Networks: Algorithms and Observations (U Kang et.al. 2011)*

ArangoDBs implementation approximates the number of shortest path in each iteration by using a HyperLogLog counter with 64 buckets. 
This should work well on large graphs and on smaller ones as well. The memory requirements should be **O(n * d)** where 
*n* is the number of vertices and *d* the diameter of your graph. Each vertex will store a counter for each iteration of the
algorithm. The algorithm can be used like this

```javascript
    var pregel = require("@arangodb/pregel");
    var handle = pregel.start("effectivecloseness", "yourgraph", {resultField: "closeness"});
```

##### LineRank

Another common measure is the [betweenness* centrality](https://en.wikipedia.org/wiki/Betweenness_centrality): 
It measures the number of times a vertex is part  of shortest paths between any pairs of vertices. 
For a vertex *v* betweenness is defined as

![Vertex Betweeness](betweeness.png)

Where the &sigma; represents the number of shortest paths between *x* and *y*, and &sigma;(v) represents the 
number of paths also passing through a vertex *v*. By intuition a vertex with higher betweeness centrality will have more information
passing through it.

**LineRank** approximates the random walk betweenness of every vertex in a graph. This is the probability that someone starting on
an arbitary vertex, will visit this node when he randomly chooses edges to visit.
The algoruthm essentially builds a line graph out of your graph (switches the vertices and edges), and then computes a score similar to PageRank.
This can be considered a scalable equivalent to vertex betweeness, which can be executed distributedly in ArangoDB. 
The algorithm is from the paper *Centralities in Large Networks: Algorithms and Observations (U Kang et.al. 2011)*

```javascript
    var pregel = require("@arangodb/pregel");
    var handle = pregel.start("linerank", "yourgraph", {"resultField": "rank"});
```


#### Community Detection

Graphs based on real world networks often have a community structure. This means it is possible to find groups of vertices such that each each vertex group is internally more densely connected than outside the group.
This has many applications when you want to analyze your networks, for example
Social networks include community groups (the origin of the term, in fact) based on common location, interests, occupation, etc.

##### Label Propagation 

*Label Propagation* can be used to implement community detection on large graphs. The idea is that each
vertex should be in the community that most of his neighbours are in. We iteratively detemine this by first
assigning random Community ID's. Then each itertation, a vertex will send it's current community ID to all his neighbor vertices. 
Then each vertex adopts the community ID he received most frequently during the iteration. 

The algorithm runs until it converges,
which likely never really happens on large graphs. Therefore you need to specify a maximum iteration bound which suits you.
The default bound is 500 iterations, which is likely too large for your application.
Should work best on undirected graphs, results on directed graphs might vary depending on the density of your graph.

```javascript
    var pregel = require("@arangodb/pregel");
    var handle = pregel.start("labelpropagation", "yourgraph", {maxGSS:100, resultField: "community"});
```

##### Speaker-Listener Label Propagation


