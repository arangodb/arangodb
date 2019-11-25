
@startDocuBlock HTTP_API_TRAVERSAL
@brief execute a server-side traversal

@RESTHEADER{POST /_api/traversal,executes a traversal}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
It is considered as deprecated from version 3.4.0 on.
It is superseded by AQL graph traversal.
{% endhint %}

@RESTDESCRIPTION
Starts a traversal starting from a given vertex and following.
edges contained in a given edgeCollection. The request must
contain the following attributes.

@RESTBODYPARAM{startVertex,string,required,string}
id of the startVertex, e.g. *"users/foo"*.

@RESTBODYPARAM{edgeCollection,string,optional, string}
name of the collection that contains the edges.

@RESTBODYPARAM{graphName,string,optional,string}
name of the graph that contains the edges.
Either *edgeCollection* or *graphName* has to be given.
In case both values are set the *graphName* is preferred.

@RESTBODYPARAM{filter,string,optional,string}
default is to include all nodes:
body (JavaScript code) of custom filter function
function signature: *(config, vertex, path) -> mixed*
can return four different string values:
- *"exclude"* -> this vertex will not be visited.
- *"prune"* -> the edges of this vertex will not be followed.
- *""* or *undefined* -> visit the vertex and follow its edges.
- *Array* -> containing any combination of the above.
  If there is at least one *"exclude"* or *"prune"* respectively
  is contained, it's effect will occur.

@RESTBODYPARAM{minDepth,string,optional,string}
ANDed with any existing filters):
visits only nodes in at least the given depth

@RESTBODYPARAM{maxDepth,string,optional,string}
ANDed with any existing filters visits only nodes in at most the given depth

@RESTBODYPARAM{visitor,string,optional,string}
body (JavaScript) code of custom visitor function
function signature: *(config, result, vertex, path, connected) -> void*
The visitor function can do anything, but its return value is ignored. To
populate a result, use the *result* variable by reference. Note that the
*connected* argument is only populated when the *order* attribute is set
to *"preorder-expander"*.

@RESTBODYPARAM{direction,string,optional,string}
direction for traversal
- *if set*, must be either *"outbound"*, *"inbound"*, or *"any"*
- *if not set*, the *expander* attribute must be specified

@RESTBODYPARAM{init,string,optional,string}
body (JavaScript) code of custom result initialization function
function signature: *(config, result) -> void*
initialize any values in result with what is required

@RESTBODYPARAM{expander,string,optional,string}
body (JavaScript) code of custom expander function
*must* be set if *direction* attribute is **not** set
function signature: *(config, vertex, path) -> array*
expander must return an array of the connections for *vertex*
each connection is an object with the attributes *edge* and *vertex*

@RESTBODYPARAM{sort,string,optional,string}
body (JavaScript) code of a custom comparison function
for the edges. The signature of this function is
*(l, r) -> integer* (where l and r are edges) and must
return -1 if l is smaller than, +1 if l is greater than,
and 0 if l and r are equal. The reason for this is the
following: The order of edges returned for a certain
vertex is undefined. This is because there is no natural
order of edges for a vertex with multiple connected edges.
To explicitly define the order in which edges on the
vertex are followed, you can specify an edge comparator
function with this attribute. Note that the value here has
to be a string to conform to the JSON standard, which in
turn is parsed as function body on the server side. Furthermore
note that this attribute is only used for the standard
expanders. If you use your custom expander you have to
do the sorting yourself within the expander code.

@RESTBODYPARAM{strategy,string,optional,string}
traversal strategy can be *"depthfirst"* or *"breadthfirst"*

@RESTBODYPARAM{order,string,optional,string}
traversal order can be *"preorder"*, *"postorder"* or *"preorder-expander"*

@RESTBODYPARAM{itemOrder,string,optional,string}
item iteration order can be *"forward"* or *"backward"*

@RESTBODYPARAM{uniqueness,string,optional,string}
specifies uniqueness for vertices and edges visited.
If set, must be an object like this:<br>
`"uniqueness": {"vertices": "none"|"global"|"path", "edges": "none"|"global"|"path"}`

@RESTBODYPARAM{maxIterations,string,optional,string}
Maximum number of iterations in each traversal. This number can be
set to prevent endless loops in traversal of cyclic graphs. When a traversal performs
as many iterations as the *maxIterations* value, the traversal will abort with an
error. If *maxIterations* is not set, a server-defined value may be used.

@RESTDESCRIPTION

If the Traversal is successfully executed *HTTP 200* will be returned.
Additionally the *result* object will be returned by the traversal.

For successful traversals, the returned JSON object has the
following properties:

- *error*: boolean flag to indicate if an error occurred (*false*
  in this case)

- *code*: the HTTP status code

- *result*: the return value of the traversal

If the traversal specification is either missing or malformed, the server
will respond with *HTTP 400*.

The body of the response will then contain a JSON object with additional error
details. The object has the following attributes:

- *error*: boolean flag to indicate that an error occurred (*true* in this case)

- *code*: the HTTP status code

- *errorNum*: the server error number

- *errorMessage*: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the traversal is fully executed
*HTTP 200* will be returned.

@RESTRETURNCODE{400}
If the traversal specification is either missing or malformed, the server
will respond with *HTTP 400*.

@RESTRETURNCODE{404}
The server will responded with *HTTP 404* if the specified edge collection
does not exist, or the specified start vertex cannot be found.

@RESTRETURNCODE{500}
The server will responded with *HTTP 500* when an error occurs inside the
traversal or if a traversal performs more than *maxIterations* iterations.

@EXAMPLES

In the following examples the underlying graph will contain five persons
*Alice*, *Bob*, *Charlie*, *Dave* and *Eve*.
We will have the following directed relations:

- *Alice* knows *Bob*
- *Bob* knows *Charlie*
- *Bob* knows *Dave*
- *Eve* knows *Alice*
- *Eve* knows *Bob*

The starting vertex will always be Alice.

Follow only outbound edges

@EXAMPLE_ARANGOSH_RUN{RestTraversalOutbound}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = {
         "startVertex": a ,
          "graphName" : g.__name,
          "direction" : "outbound"};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Follow only inbound edges

@EXAMPLE_ARANGOSH_RUN{RestTraversalInbound}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = { "startVertex": a,
                 "graphName" : g.__name,
                 "direction" : "inbound"};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Follow any direction of edges

@EXAMPLE_ARANGOSH_RUN{RestTraversalAny}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = {
      startVertex: a,
      graphName: g.__name,
      direction: "any",
      uniqueness: {
        vertices: "none",
        edges: "global"
      }
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Excluding *Charlie* and *Bob*

@EXAMPLE_ARANGOSH_RUN{RestTraversalFilterExclude}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var filter  = 'if (vertex.name === "Bob" || '+
                  '    vertex.name === "Charlie") {'+
                  '  return "exclude";'+
                  '}'+
                  'return;';

    var body = {
         "startVertex" : a,
         "graphName"   : g.__name,
         "direction"   : "outbound",
         "filter"      : filter};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Do not follow edges from *Bob*

@EXAMPLE_ARANGOSH_RUN{RestTraversalFilterPrune}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var filter = 'if (vertex.name === "Bob") {'+
                 'return "prune";' +
                 '}' +
                 'return;';

    var body = { "startVertex": a,
                 "graphName" : g.__name,
                 "direction" : "outbound",
                 "filter" : filter};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Visit only nodes in a depth of at least 2

@EXAMPLE_ARANGOSH_RUN{RestTraversalMinDepth}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = { "startVertex": a,
                 "graphName" : g.__name,
                 "direction" : "outbound",
                 "minDepth" : 2};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Visit only nodes in a depth of at most 1

@EXAMPLE_ARANGOSH_RUN{RestTraversalMaxDepth}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = { "startVertex" : a,
                 "graphName"   : g.__name,
                 "direction"   : "outbound",
                 "maxDepth"    : 1};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Using a visitor function to return vertex ids only

@EXAMPLE_ARANGOSH_RUN{RestTraversalVisitorFunc}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = { "startVertex": a,
                 "graphName" : g.__name,
                 "direction" : "outbound",
                 "visitor" : "result.visited.vertices.push(vertex._id);"};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Count all visited nodes and return a list of nodes only

@EXAMPLE_ARANGOSH_RUN{RestTraversalVisitorCountAndList}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = { "startVertex": a,
                 "graphName" : g.__name,
                 "direction" : "outbound",
                 "init" : "result.visited = 0; result.myVertices = [ ];",
                 "visitor" : "result.visited++; result.myVertices.push(vertex);"};

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Expand only inbound edges of *Alice* and outbound edges of *Eve*

@EXAMPLE_ARANGOSH_RUN{RestTraversalVisitorExpander}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = {
      startVertex: a,
      graphName: g.__name,
      expander: "var connections = [ ];" +
                "if (vertex.name === \"Alice\") {" +
                "config.datasource.getInEdges(vertex).forEach(function (e) {" +
                "connections.push({ " +
                "vertex: require(\"internal\").db._document(e._from), " +
                "edge: e" +
                "});" +
                "});" +
                "}" +
                "if (vertex.name === \"Eve\") {" +
                "config.datasource.getOutEdges(vertex).forEach(function (e) {" +
                "connections.push({" +
                "vertex: require(\"internal\").db._document(e._to), " +
                "edge: e" +
                "});" +
                "});" +
                "}" +
                "return connections;"
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Follow the *depthfirst* strategy

@EXAMPLE_ARANGOSH_RUN{RestTraversalDepthFirst}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = {
      startVertex: a,
      graphName: g.__name,
      direction: "any",
      strategy: "depthfirst"
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Using *postorder* ordering

@EXAMPLE_ARANGOSH_RUN{RestTraversalPostorder}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = {
      startVertex: a,
      graphName: g.__name,
      direction: "any",
      order: "postorder"
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Using *backward* item-ordering:

@EXAMPLE_ARANGOSH_RUN{RestTraversalBackwardItemOrder}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = {
      startVertex: a,
      graphName: g.__name,
      direction: "any",
      itemOrder: "backward"
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

Edges should only be included once globally,
but nodes are included every time they are visited

@EXAMPLE_ARANGOSH_RUN{RestTraversalEdgeUniqueness}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var url = "/_api/traversal";
    var body = {
      startVertex: a,
      graphName: g.__name,
      direction: "any",
      uniqueness: {
        vertices: "none",
        edges: "global"
      }
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 200);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

If the underlying graph is cyclic, *maxIterations* should be set

The underlying graph has two vertices *Alice* and *Bob*.
With the directed edges:

- *Alice* knows *Bob*
- *Bob* knows *Alice*

@EXAMPLE_ARANGOSH_RUN{RestTraversalMaxIterations}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var g = examples.loadGraph("knows_graph");
    var a = g.persons.document("alice")._id;
    var b = g.persons.document("bob")._id;
    g.knows.truncate();
    g.knows.save(a, b, {});
    g.knows.save(b, a, {});
    var url = "/_api/traversal";
    var body = {
      startVertex: a,
      graphName: g.__name,
      direction: "any",
      uniqueness: {
        vertices: "none",
        edges: "none"
      },
      maxIterations: 5
    };

    var response = logCurlRequest('POST', url, body);
    assert(response.code === 500);

    logJsonResponse(response);
    examples.dropGraph("knows_graph");
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
