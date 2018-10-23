Combining Graph Traversals
==========================

Finding the start vertex via a geo query
----------------------------------------

Our first example will locate the start vertex for a graph traversal via [a geo index](../../Manual/Indexing/Geo.html).
We use [the city graph](../../Manual/Graphs/index.html#the-city-graph) and its geo indices:

![Cities Example Graph](../../Manual/Graphs/cities_graph.png)

    @startDocuBlockInline COMBINING_GRAPH_01_create_graph
    @EXAMPLE_ARANGOSH_OUTPUT{COMBINING_GRAPH_01_create_graph}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("routeplanner");
    ~examples.dropGraph("routeplanner");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock COMBINING_GRAPH_01_create_graph

We search all german cities in a range of 400 km around the ex-capital **Bonn**: **Hamburg** and **Cologne**.
We won't find **Paris** since its in the `frenchCity` collection.

    @startDocuBlockInline COMBINING_GRAPH_02_show_geo
    @EXAMPLE_AQL{COMBINING_GRAPH_02_show_geo}
    @DATASET{routeplanner}
    FOR startCity IN germanCity
      FILTER GEO_DISTANCE(@bonn, startCity.geometry) < @radius
        RETURN startCity._key
    @BV {
      bonn: [7.0998, 50.7340],
      radius: 400000
    }
    @END_EXAMPLE_AQL
    @endDocuBlock COMBINING_GRAPH_02_show_geo

Lets revalidate that the geo indices are actually used:

    @startDocuBlockInline COMBINING_GRAPH_03_explain_geo
    @EXAMPLE_AQL{COMBINING_GRAPH_03_explain_geo}
    @DATASET{routeplanner}
    @EXPLAIN{TRUE}
    FOR startCity IN germanCity
      FILTER GEO_DISTANCE(@bonn, startCity.geometry) < @radius
        RETURN startCity._key
    @BV {
      bonn: [7.0998, 50.7340],
      radius: 400000
    }
    @END_EXAMPLE_AQL
    @endDocuBlock COMBINING_GRAPH_03_explain_geo

And now combine this with a graph traversal:

    @startDocuBlockInline COMBINING_GRAPH_04_combine
    @EXAMPLE_AQL{COMBINING_GRAPH_04_combine}
    @DATASET{routeplanner}
    FOR startCity IN germanCity
      FILTER GEO_DISTANCE(@bonn, startCity.geometry) < @radius
        FOR v, e, p IN 1..1 OUTBOUND startCity
          GRAPH 'routeplanner'
        RETURN {startcity: startCity._key, traversedCity: v._key}
    @BV {
      bonn: [7.0998, 50.7340],
      radius: 400000
    }
    @END_EXAMPLE_AQL
    @endDocuBlock COMBINING_GRAPH_04_combine

The geo index query returns us `startCity` (**Cologne** and **Hamburg**) which we then use as starting point for our graph traversal.
For simplicity we only return their direct neighbours. We format the return result so we can see from which `startCity` the traversal came.

Alternatively we could use a `LET` statement with a subquery to group the traversals by their `startCity` efficiently:

    @startDocuBlockInline COMBINING_GRAPH_05_combine_let
    @EXAMPLE_AQL{COMBINING_GRAPH_05_combine_let}
    @DATASET{routeplanner}
    FOR startCity IN germanCity
      FILTER GEO_DISTANCE(@bonn, startCity.geometry) < @radius
        LET oneCity = (
          FOR v, e, p IN 1..1 OUTBOUND startCity
            GRAPH 'routeplanner' RETURN v._key
        )
          RETURN {startCity: startCity._key, connectedCities: oneCity}
    @BV {
      bonn: [7.0998, 50.7340],
      radius: 400000
    }
    @END_EXAMPLE_AQL
    @endDocuBlock COMBINING_GRAPH_05_combine_let

Finally, we clean up again:

    @startDocuBlockInline COMBINING_GRAPH_06_cleanup
    @EXAMPLE_ARANGOSH_OUTPUT{COMBINING_GRAPH_06_cleanup}
    ~var examples = require("@arangodb/graph-examples/example-graph.js");
    ~var g = examples.loadGraph("routeplanner");
    examples.dropGraph("routeplanner");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock COMBINING_GRAPH_06_cleanup
