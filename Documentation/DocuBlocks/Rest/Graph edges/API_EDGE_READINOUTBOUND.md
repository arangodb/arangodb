
@startDocuBlock API_EDGE_READINOUTBOUND
@brief get edges

@RESTHEADER{GET /_api/edges/{collection-id}, Read in- or outbound edges}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-id,string,required}
The id of the collection.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{vertex,string,required}
The id of the start vertex.

@RESTQUERYPARAM{direction,string,optional}
Selects *in* or *out* direction for edges. If not set, any edges are
returned.

@RESTDESCRIPTION
Returns an array of edges starting or ending in the vertex identified by
*vertex-handle*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the edge collection was found and edges were retrieved.

@RESTRETURNCODE{400}
is returned if the request contains invalid parameters.

@RESTRETURNCODE{404}
is returned if the edge collection was not found.

@EXAMPLES

Any direction

@EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesAny}
    var Graph = require("@arangodb/graph-blueprint").Graph;
    var g = new Graph("graph", "vertices", "edges");
    var v1 = g.addVertex(1);
    var v2 = g.addVertex(2);
    var v3 = g.addVertex(3);
    var v4 = g.addVertex(4);
    g.addEdge(v1, v3, 5, "v1 -> v3");
    g.addEdge(v2, v1, 6, "v2 -> v1");
    g.addEdge(v4, v1, 7, "v4 -> v1");

    var url = "/_api/edges/edges?vertex=vertices/1";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop("edges");
    db._drop("vertices");
    db._graphs.remove("graph");
@END_EXAMPLE_ARANGOSH_RUN

In edges

@EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesIn}
    var Graph = require("@arangodb/graph-blueprint").Graph;
    var g = new Graph("graph", "vertices", "edges");
    var v1 = g.addVertex(1);
    var v2 = g.addVertex(2);
    var v3 = g.addVertex(3);
    var v4 = g.addVertex(4);
    g.addEdge(v1, v3, 5, "v1 -> v3");
    g.addEdge(v2, v1, 6, "v2 -> v1");
    g.addEdge(v4, v1, 7, "v4 -> v1");

    var url = "/_api/edges/edges?vertex=vertices/1&direction=in";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop("edges");
    db._drop("vertices");
    db._graphs.remove("graph");
@END_EXAMPLE_ARANGOSH_RUN

Out edges

@EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesOut}
    var Graph = require("@arangodb/graph-blueprint").Graph;
    var g = new Graph("graph", "vertices", "edges");
    var v1 = g.addVertex(1);
    var v2 = g.addVertex(2);
    var v3 = g.addVertex(3);
    var v4 = g.addVertex(4);
    g.addEdge(v1, v3, 5, "v1 -> v3");
    g.addEdge(v2, v1, 6, "v2 -> v1");
    g.addEdge(v4, v1, 7, "v4 -> v1");

    var url = "/_api/edges/edges?vertex=vertices/1&direction=out";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop("edges");
    db._drop("vertices");
    db._graphs.remove("graph");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

