!CHAPTER Combining Graph Traversals
!SUBSECTION Finding the start vertex via a geo query
Our first example will locate the start vertex for a graph traversal via [a geo index](../../Manual/Indexing/Geo.html).
We use [the city graph](../../Manual/Graphs/index.html#the-city-graph) and its geo indices:

![Cities Example Graph](../../Manual/Graphs/cities_graph.png)


    @startDocuBlockInline COMBINING_GRAPH_01_create_graph
    @EXAMPLE_ARANGOSH_OUTPUT{COMBINING_GRAPH_01_create_graph}
    ~addIgnoreCollection("germanHighway");
    ~addIgnoreCollection("germanCity");
    ~addIgnoreCollection("frenchHighway");
    ~addIgnoreCollection("frenchCity");
    ~addIgnoreCollection("internationalHighway");
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("routeplanner");
    var bonn=[50.7340, 7.0998];
    |db._query(`FOR startCity IN
    |             WITHIN(germanCity, @lat, @long, @radius)
    |               RETURN startCity`,
    |   {lat: bonn[0], long: bonn[1], radius: 400000}
    ).toArray()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock COMBINING_GRAPH_01_create_graph

We search all german cities in a range of 400 km around the ex-capital **Bonn**: **Hamburg** and **Cologne**.
We won't find **Paris** since its in the `frenchCity` collection.

    @startDocuBlockInline COMBINING_GRAPH_02_combine
    @EXAMPLE_ARANGOSH_OUTPUT{COMBINING_GRAPH_02_combine}
    ~var bonn=[50.7340, 7.0998]
    |db._query(`FOR startCity IN
    |             WITHIN(germanCity, @lat, @long, @radius)
    |               FOR v, e, p IN 1..1 OUTBOUND startCity
    |                 GRAPH 'routeplanner'
    |     RETURN {startcity: startCity._key, traversedCity: v}`,
    |{
    |  lat: bonn[0],
    |  long: bonn[1],
    |  radius: 400000
    } ).toArray()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock COMBINING_GRAPH_02_combine

The geo index query returns us `startCity` (**Cologne** and **Hamburg**) which we then use as starting point for our graph traversal. For simplicity we only return their direct neighbours. We format the return result so we can see from which `startCity` the traversal came.

Alternatively we could use a `LET` statement with a subquery to group the traversals by their `startCity` efficiently:

    @startDocuBlockInline COMBINING_GRAPH_03_combine_let
    @EXAMPLE_ARANGOSH_OUTPUT{COMBINING_GRAPH_03_combine_let}
    ~var bonn=[50.7340, 7.0998];
    |db._query(`FOR startCity IN
    |            WITHIN(germanCity, @lat, @long, @radius)
    |              LET oneCity = (FOR v, e, p IN 1..1 OUTBOUND startCity
    |                GRAPH 'routeplanner' RETURN v)
    |              return {startCity: startCity._key, connectedCities: oneCity}`,
    |{
    |  lat: bonn[0],
    |  long: bonn[1],
    |  radius: 400000
    } ).toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock COMBINING_GRAPH_03_combine_let

Finally, we clean up again:

    @startDocuBlockInline COMBINING_GRAPH_04_cleanup
    @EXAMPLE_ARANGOSH_OUTPUT{COMBINING_GRAPH_04_cleanup}
    ~var examples = require("@arangodb/graph-examples/example-graph.js");
    examples.dropGraph("routeplanner");
    ~removeIgnoreCollection("germanHighway");
    ~removeIgnoreCollection("germanCity");
    ~removeIgnoreCollection("frenchHighway");
    ~removeIgnoreCollection("frenchCity");
    ~removeIgnoreCollection("internationalHighway");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock COMBINING_GRAPH_04_cleanup
