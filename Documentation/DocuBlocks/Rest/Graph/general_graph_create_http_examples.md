@startDocuBlock general_graph_create_http_examples
@brief Create a new graph in the graph module.

@RESTHEADER{POST /_api/gharial, Create a graph}

@RESTDESCRIPTION
The creation of a graph requires the name of the graph and a
definition of its edges.
[See also edge definitions](../../Manual/Graphs/GeneralGraphs/Management.html#edge-definitions).

@RESTBODYPARAM{name,string,required,string}
Name of the graph.

@RESTBODYPARAM{edgeDefinitions,string,optional,string}
An array of definitions for the edge

@RESTBODYPARAM{orphanCollections,string,optional,string}
An array of additional vertex collections.

@RESTBODYPARAM{isSmart,boolean,optional,boolean}
Define if the created graph should be smart.
This only has effect in Enterprise Edition.

@RESTBODYPARAM{options,object,optional,post_api_gharial_create_opts}
a JSON object which is only useful in Enterprise Edition and with isSmart set to true.
It can contain the following attributes:

@RESTSTRUCT{smartGraphAttribute,post_api_gharial_create_opts,string,required,}
The attribute name that is used to smartly shard the vertices of a graph.
Every vertex in this Graph has to have this attribute.
Cannot be modified later.

@RESTSTRUCT{numberOfShards,post_api_gharial_create_opts,integer,required,}
The number of shards that is used for every collection within this graph.
Cannot be modified later.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Is returned if the graph could be created and waitForSync is enabled
for the `_graphs` collection.  The response body contains the
graph configuration that has been stored.

@RESTRETURNCODE{202}
Is returned if the graph could be created and waitForSync is disabled
for the `_graphs` collection. The response body contains the
graph configuration that has been stored.

@RESTRETURNCODE{409}
Returned if there is a conflict storing the graph.  This can occur
either if a graph with this name is already stored, or if there is one
edge definition with a the same
[edge collection](../../Manual/Appendix/Glossary.html#edge-collection) but a
different signature used in any other graph.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialCreate}
  var graph = require("@arangodb/general-graph");
| if (graph._exists("myGraph")) {
|    graph._drop("myGraph", true);
  }
  var url = "/_api/gharial";
  body = {
    name: "myGraph",
    edgeDefinitions: [{
      collection: "edges",
      from: [ "startVertices" ],
      to: [ "endVertices" ]
    }]
  };

  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);

  graph._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{HttpGharialCreate2}
  var graph = require("@arangodb/general-graph");
| if (graph._exists("myGraph")) {
|    graph._drop("myGraph", true);
  }
  var url = "/_api/gharial";
  body = {
    name: "myGraph",
    edgeDefinitions: [{
      collection: "edges",
      from: [ "startVertices" ],
      to: [ "endVertices" ]
    }],
    isSmart: true,
    options: {
      numberOfShards: 9,
      smartGraphAttribute: "region"
    }
  };

  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);

  graph._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
