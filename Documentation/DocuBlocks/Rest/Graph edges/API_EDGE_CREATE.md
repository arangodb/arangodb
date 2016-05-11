
@startDocuBlock API_EDGE_CREATE
@brief creates an edge

@RESTHEADER{POST /_api/edge,Create edge}

@RESTALLBODYPARAM{edge-document,json,required}
A JSON representation of the edge document must be passed as the body of
the POST request. This JSON object may contain the edge's document key in
the *_key* attribute if needed.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
Creates a new edge in the collection identified by *collection* name.

@RESTQUERYPARAM{createCollection,boolean,optional}
If this parameter has a value of *true* or *yes*, then the collection is
created if it does not yet exist. Other values will be ignored so the
collection must be present for the operation to succeed.

**Note**: This flag is not supported in a cluster. Using it will result in an
error.

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

The *from* and *to* handles are immutable once the edge has been created.

In all other respects the method works like *POST /document*.

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
    var Graph = require("@arangodb/graph-blueprint").Graph;
    var g = new Graph("graph", "vertices", "edges");
    g.addVertex(1);
    g.addVertex(2);
    var url = "/_api/edge/?collection=edges&from=vertices/1&to=vertices/2";

    var response = logCurlRequest("POST", url, { "name": "Emil" });

    assert(response.code === 202);

    logJsonResponse(response);
    var body = response.body.replace(/\\/g, '');
    var edge_id = JSON.parse(body)._id;
    var response2 = logCurlRequest("GET", "/_api/edge/" + edge_id);

    assert(response2.code === 200);

    logJsonResponse(response2);
    db._drop("edges");
    db._drop("vertices");
    db._graphs.remove("graph");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

