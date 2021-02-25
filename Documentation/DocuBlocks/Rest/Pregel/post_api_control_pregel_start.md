@startDocuBlock post_api_control_pregel_start
@brief Start the execution of a Pregel algorithm

@RESTHEADER{POST /_api/control_pregel, Start Pregel execution}

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

@RESTBODYPARAM{vertexCollections,array,optional,string}
List of vertex collection names.

@RESTBODYPARAM{edgeCollections,array,optional,string}
List of edge collection names.

@RESTBODYPARAM{params,object,optional,}
General as well as algorithm-specific options.

@RESTDESCRIPTION
To start an execution you need to specify the algorithm name and a named graph
(SmartGraph in cluster). Alternatively you can specify the vertex and edge
collections. Additionally you can specify custom parameters which vary for each
algorithm, see [Pregel - Available Algorithms](https://www.arangodb.com/docs/stable/graphs-pregel.html#available-algorithms).

@RESTRETURNCODES

@RESTRETURNCODE{200}
TODO

@RESTRETURNCODE{XXX}
TODO

@EXAMPLES

Strongly connected components

@EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesAny}

  // TODO: make this an example graph?

  var gm = require("@arangodb/general-graph");

  var v = 'pregel_components';
  var e = 'pregel_connections';
  var g = 'pregel_cc';

| var edges = [
|   ["A1", "A2"],
|   ["A2", "A3"],
|   ["A3", "A4"],
|   ["A4", "A1"],
|   ["B1", "B3"],
|   ["B2", "B4"],
|   ["B3", "B6"],
|   ["B4", "B3"],
|   ["B4", "B5"],
|   ["B6", "B7"],
|   ["B7", "B8"],
|   ["B7", "B9"],
|   ["B7", "B10"],
|   ["B7", "B19"],
|   ["B11", "B10"],
|   ["B12", "B11"],
|   ["B13", "B12"],
|   ["B13", "B20"],
|   ["B14", "B13"],
|   ["B15", "B14"],
|   ["B15", "B16"],
|   ["B17", "B15"],
|   ["B17", "B18"],
|   ["B19", "B17"],
|   ["B20", "B21"],
|   ["B20", "B22"],
|   ["C1", "C2"],
|   ["C2", "C3"],
|   ["C3", "C4"],
|   ["C4", "C5"],
|   ["C4", "C7"],
|   ["C5", "C6"],
|   ["C5", "C7"],
|   ["C7", "C8"],
|   ["C8", "C9"],
|   ["C8", "C10"],
  ];

  var vertices = new Set(edges.flat());

  var graph = gm._create(g, [gm._relation(e, v, v)]);

| vertices.forEach(vertex => {
|   graph[v].save({ _key: vertex });
  });

| edges.forEach(([from, to]) => {
|   graph[e].save({ _from: `${v}/${from}`, _to: `${v}/${to}` });
  });

  var url = "/_api/control_pregel";
| var body = {
|   algorithm: 'scc',
|   graphName: g,
|   params: {
|     maxGSS: graph[v].count(),
|     resultField: 'component',
|   }
  }
  var response = logCurlRequest('POST', url, body);

  assert(response.code === 200);

  logJsonResponse(response);
~ gm._drop(g, true);

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
