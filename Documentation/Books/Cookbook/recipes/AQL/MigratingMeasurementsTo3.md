# Migrating GRAPH_* Measurements from 2.8 or earlier to 3.0

## Problem

With the release of 3.0 all GRAPH functions have been dropped from AQL in favor of a more
native integration of graph features into the query language. I have used the old graph
functions and want to upgrade to 3.0.

Graph functions covered in this recipe:

* GRAPH_ABSOLUTE_BETWEENNESS
* GRAPH_ABSOLUTE_CLOSENESS
* GRAPH_ABSOLUTE_ECCENTRICITY
* GRAPH_BETWEENNESS
* GRAPH_CLOSENESS
* GRAPH_DIAMETER
* GRAPH_ECCENTRICITY
* GRAPH_RADIUS

## Solution 1 User Defined Funtions

### Registering user-defined functions

This step has to be executed once on ArangoDB for every database we are using.

We connect to `arangodb` with `arangosh` to issue the following commands two:

```js
var graphs = require("@arangodb/general-graph");
graphs._registerCompatibilityFunctions();
```

These have registered all old `GRAPH_*` functions as user-defined functions again, with the prefix `arangodb::`.

### Modify the application code

Next we have to go through our application code and replace all calls to `GRAPH_*` by `arangodb::GRAPH_*`.
Now run a testrun of our application and check if it worked.
If it worked we are ready to go.

### Important Information

The user defined functions will call translated subqueries (as described in Solution 2).
The optimizer does not know anything about these subqueries beforehand and cannot optimize the whole plan.
Also there might be read/write constellations that are forbidden in user-defined functions, therefore
a "really" translated query may work while the user-defined function work around may be rejected.

## Solution 2 Foxx (recommended)

The general graph module still offers the measurement functions.
As these are typically computation expensive and create long running queries it is recommended
to not use them in combination with other AQL features.
Therefore the best idea is to offer these measurements directly via an API using FOXX.

First we create a new [Foxx service](https://docs.arangodb.com/3/Manual/Foxx/index.html).
Then we include the `general-graph` module in the service.
For every measurement we need we simply offer a GET route to read this measurement.

As an example we do the `GRAPH_RADIUS`:

```
/// ADD FOXX CODE ABOVE

const joi = require('joi');
const createRouter = require('@arangodb/foxx/router');
const dd = require('dedent');
const router = createRouter();

const graphs = require("@arangodb/general-graph");

router.get('/radius/:graph', function(req, res) {
  let graph;

  // Load the graph
  try {
    graph = graphs._graph(req.graph);
  } catch (e) {
    res.throw('not found');
  }
  res.json(graphs._radius()); // Return the radius
})
.pathParam('graph', joi.string().required(), 'The name of the graph')
.error('not found', 'Graph with this name does not exist.')
.summary('Compute the Radius')
.description(dd`
  This function computes the radius of the given graph
  and returns it.
`);
```



**Author:** [Michael Hackstein](https://github.com/mchacki)

**Tags**: #howto #aql #migration
