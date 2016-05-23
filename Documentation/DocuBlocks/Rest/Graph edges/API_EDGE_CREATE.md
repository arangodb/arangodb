
@startDocuBlock API_EDGE_CREATE
@brief creates an edge

@RESTHEADER{POST /_api/document,Create edge}

@RESTALLBODYPARAM{edge-document,json,required}
A JSON representation of the edge document must be passed as the body of
the POST request. This JSON object may contain the edge's document key in
the *_key* attribute if needed.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
Creates a new edge in the collection identified by *collection* name.

@RESTQUERYPARAM{waitForSync,boolean,optional}
Wait until the edge document has been synced to disk.

@RESTQUERYPARAM{from,string,required}
The document handle of the start point must be passed in *from* handle.

@RESTQUERYPARAM{to,string,required}
The document handle of the end point must be passed in *to* handle.

@RESTDESCRIPTION
Creates a new edge document in the collection named *collection*. A JSON
representation of the document must be passed as the body of the POST
request.

In all other respects the method works like *POST /_api/document* for
documents.

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the edge was created successfully and *waitForSync* was
*true*.

@RESTRETURNCODE{202}
is returned if the edge was created successfully and *waitForSync* was
*false*.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation of an
edge, or if the collection specified is not an edge collection.
The response body contains an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown.  The
response body contains an error document in this case.

@EXAMPLES

Create an edge and read it back:

@EXAMPLE_ARANGOSH_RUN{RestEdgeCreateEdge}
    try {
    db._graphs.remove("graph");
    db._drop("edges");
    db._drop("vertices");
    }
    catch (e) {}
    
    var Graph = require("@arangodb/general-graph");
    var g = Graph._create("graph", [Graph._relation("edges", "vertices", "vertices")]);
    g.vertices.save({_key: "1"});
    g.vertices.save({_key: "2"});
    var url = "/_api/document/?collection=edges";

    var response = logCurlRequest("POST", url, { "name": "Emil", _from: "vertices/1", _to: "vertices/2"});

    assert(response.code === 202);

    logJsonResponse(response);
    var body = response.body.replace(/\\/g, '');
    var edge_id = JSON.parse(body)._id;
    var response2 = logCurlRequest("GET", "/_api/document/" + edge_id);

    assert(response2.code === 200);

    logJsonResponse(response2);
    db._drop("edges");
    db._drop("vertices");
    db._graphs.remove("graph");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

