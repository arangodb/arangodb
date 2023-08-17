@startDocuBlock post_api_control_pregel

@RESTHEADER{POST /_api/control_pregel, Start a Pregel job execution, createPregelJob}

@RESTBODYPARAM{algorithm,string,required,string}
Name of the algorithm. One of:
- `"pagerank"` - Page Rank
- `"sssp"` - Single-Source Shortest Path
- `"connectedcomponents"` - Connected Components
- `"wcc"` - Weakly Connected Components
- `"scc"` - Strongly Connected Components
- `"hits"` - Hyperlink-Induced Topic Search
- `"effectivecloseness"` - Effective Closeness
- `"linerank"` - LineRank
- `"labelpropagation"` - Label Propagation
- `"slpa"` - Speaker-Listener Label Propagation

@RESTBODYPARAM{graphName,string,optional,string}
Name of a graph. Either this or the parameters `vertexCollections` and
`edgeCollections` are required.
Please note that there are special sharding requirements for graphs in order
to be used with Pregel.

@RESTBODYPARAM{vertexCollections,array,optional,string}
List of vertex collection names.
Please note that there are special sharding requirements for collections in order
to be used with Pregel.

@RESTBODYPARAM{edgeCollections,array,optional,string}
List of edge collection names.
Please note that there are special sharding requirements for collections in order
to be used with Pregel.

@RESTBODYPARAM{params,object,optional,}
General as well as algorithm-specific options.

The most important general option is "store", which controls whether the results
computed by the Pregel job are written back into the source collections or not.

Another important general option is "parallelism", which controls the number of
parallel threads that work on the Pregel job at most. If "parallelism" is not
specified, a default value may be used. In addition, the value of "parallelism"
may be effectively capped at some server-specific value.

The option "useMemoryMaps" controls whether to use disk based files to store
temporary results. This might make the computation disk-bound, but allows you to
run computations which would not fit into main memory. It is recommended to set
this flag for larger datasets.

The attribute "shardKeyAttribute" specifies the shard key that edge collections are
sharded after (default: `"vertex"`).

@RESTDESCRIPTION
To start an execution you need to specify the algorithm name and a named graph
(SmartGraph in cluster). Alternatively you can specify the vertex and edge
collections. Additionally you can specify custom parameters which vary for each
algorithm, see [Pregel - Available Algorithms](https://www.arangodb.com/docs/stable/graphs-pregel-algorithms.html).

@RESTRETURNCODES

@RESTRETURNCODE{200}
HTTP 200 is returned in case the Pregel was successfully created and the reply
body is a string with the `id` to query for the status or to cancel the
execution.

@RESTRETURNCODE{400}
An HTTP 400 error is returned if the set of collections for the Pregel job includes
a system collection, or if the collections to not conform to the sharding requirements
for Pregel jobs.

@RESTRETURNCODE{403}
An HTTP 403 error is returned if there are not sufficient privileges to access
the collections specified for the Pregel job.


@RESTRETURNCODE{404}
An HTTP 404 error is returned if the specified "algorithm" is not found, or the
graph specified in "graphName" is not found, or at least one the collections 
specified in "vertexCollections" or "edgeCollections" is not found.

@EXAMPLES

Run the Weakly Connected Components (WCC) algorithm against a graph and store
the results in the vertices as attribute `component`:

@EXAMPLE_ARANGOSH_RUN{RestPregelStartConnectedComponents}

  var examples = require("@arangodb/graph-examples/example-graph.js");
  print("1. Creating Pregel graph");
  var graph = examples.loadGraph("connectedComponentsGraph");

  var url = "/_api/control_pregel";
  var body = {
    algorithm: "wcc",
    graphName: "connectedComponentsGraph",
    params: {
      maxGSS: graph.components.count(),
      resultField: "component",
    }
  }
  var response = logCurlRequest("POST", url, body);

  assert(response.code === 200);

  logJsonResponse(response);

  var id = response.parsedBody;
  var url = "/_api/control_pregel/" + id;
  while (true) {
    var status = internal.arango.GET(url);
    if (["done", "canceled", "fatal error"].includes(status.state)) {
      assert(status.state == "done");
      break;
    } else {
      print(`1. Waiting for Pregel job ${id} (${status.state})...`);
      internal.sleep(0.5);
    }
  }
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
