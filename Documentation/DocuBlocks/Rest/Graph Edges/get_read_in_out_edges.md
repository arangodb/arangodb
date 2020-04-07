@startDocuBlock get_read_in_out_edges
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
*vertex*.

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
    var db = require("@arangodb").db;
    db._create("vertices");
    db._createEdgeCollection("edges");

    db.vertices.save({_key: "1"});
    db.vertices.save({_key: "2"});
    db.vertices.save({_key: "3"});
    db.vertices.save({_key: "4"});

    db.edges.save({_from: "vertices/1", _to: "vertices/3", _key: "5", "$label": "v1 -> v3"});
    db.edges.save({_from: "vertices/2", _to: "vertices/1", _key: "6", "$label": "v2 -> v1"});
    db.edges.save({_from: "vertices/4", _to: "vertices/1", _key: "7", "$label": "v4 -> v1"});

    var url = "/_api/edges/edges?vertex=vertices/1";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop("edges");
    db._drop("vertices");
@END_EXAMPLE_ARANGOSH_RUN

In edges

@EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesIn}
    var db = require("@arangodb").db;
    db._create("vertices");
    db._createEdgeCollection("edges");

    db.vertices.save({_key: "1"});
    db.vertices.save({_key: "2"});
    db.vertices.save({_key: "3"});
    db.vertices.save({_key: "4"});

    db.edges.save({_from: "vertices/1", _to: "vertices/3", _key: "5", "$label": "v1 -> v3"});
    db.edges.save({_from: "vertices/2", _to: "vertices/1", _key: "6", "$label": "v2 -> v1"});
    db.edges.save({_from: "vertices/4", _to: "vertices/1", _key: "7", "$label": "v4 -> v1"});

    var url = "/_api/edges/edges?vertex=vertices/1&direction=in";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop("edges");
    db._drop("vertices");
@END_EXAMPLE_ARANGOSH_RUN

Out edges

@EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesOut}
    var db = require("@arangodb").db;
    db._create("vertices");
    db._createEdgeCollection("edges");

    db.vertices.save({_key: "1"});
    db.vertices.save({_key: "2"});
    db.vertices.save({_key: "3"});
    db.vertices.save({_key: "4"});

    db.edges.save({_from: "vertices/1", _to: "vertices/3", _key: "5", "$label": "v1 -> v3"});
    db.edges.save({_from: "vertices/2", _to: "vertices/1", _key: "6", "$label": "v2 -> v1"});
    db.edges.save({_from: "vertices/4", _to: "vertices/1", _key: "7", "$label": "v4 -> v1"});

    var url = "/_api/edges/edges?vertex=vertices/1&direction=out";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop("edges");
    db._drop("vertices");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
