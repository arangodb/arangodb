@startDocuBlock general_graph_create_http_examples
@brief Create a new graph in the graph module.

@RESTHEADER{POST /_api/gharial, Create a graph}

@RESTDESCRIPTION
The creation of a graph requires the name of the graph and a
definition of its edges.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{waitForSync,boolean,optional}
define if the request should wait until everything is synced to disc.
Will change the success response code.

@RESTBODYPARAM{name,string,required,string}
Name of the graph.

@RESTBODYPARAM{edgeDefinitions,array,optional,graph_edge_definition}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTBODYPARAM{orphanCollections,array,optional,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTBODYPARAM{isSmart,boolean,optional,}
Define if the created graph should be smart (Enterprise Edition only).

@RESTBODYPARAM{isDisjoint,boolean,optional,}
Whether to create a Disjoint SmartGraph instead of a regular SmartGraph
(Enterprise Edition only).

@RESTBODYPARAM{options,object,optional,post_api_gharial_create_opts}
a JSON object to define options for creating collections within this graph.
It can contain the following attributes:

@RESTSTRUCT{smartGraphAttribute,post_api_gharial_create_opts,string,optional,}
Only has effect in Enterprise Edition and it is required if isSmart is true.
The attribute name that is used to smartly shard the vertices of a graph.
Every vertex in this SmartGraph has to have this attribute.
Cannot be modified later.

@RESTSTRUCT{satellites,post_api_gharial_create_opts,array,optional,string}
An array of collection names that will be used to create SatelliteCollections
for a Hybrid (Disjoint) SmartGraph (Enterprise Edition only). Each array element
must be a string and a valid collection name. The collection type cannot be
modified later.

@RESTSTRUCT{numberOfShards,post_api_gharial_create_opts,integer,required,}
The number of shards that is used for every collection within this graph.
Cannot be modified later.

@RESTSTRUCT{replicationFactor,post_api_gharial_create_opts,integer,required,}
The replication factor used when initially creating collections for this graph.
Can be set to `"satellite"` to create a SatelliteGraph, which will ignore
*numberOfShards*, *minReplicationFactor* and *writeConcern*
(Enterprise Edition only).

@RESTSTRUCT{writeConcern,post_api_gharial_create_opts,integer,optional,}
Write concern for new collections in the graph.
It determines how many copies of each shard are required to be
in sync on the different DB-Servers. If there are less then these many copies
in the cluster a shard will refuse to write. Writes to shards with enough
up-to-date copies will succeed at the same time however. The value of
*writeConcern* can not be larger than *replicationFactor*. _(cluster only)_

@RESTRETURNCODES

@RESTRETURNCODE{201}
Is returned if the graph could be created and waitForSync is enabled
for the `_graphs` collection, or given in the request.
The response body contains the graph configuration that has been stored.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the newly created graph.

@RESTRETURNCODE{202}
Is returned if the graph could be created and waitForSync is disabled
for the `_graphs` collection and not given in the request.
The response body contains the graph configuration that has been stored.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is false in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{graph,object,required,graph_representation}
The information about the newly created graph.

@RESTRETURNCODE{400}
Returned if the request is in a wrong format.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{403}
Returned if your user has insufficient rights.
In order to create a graph you at least need to have the following privileges:

1. `Administrate` access on the Database.
2. `Read Only` access on every collection used within this graph.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@RESTRETURNCODE{409}
Returned if there is a conflict storing the graph. This can occur
either if a graph with this name is already stored, or if there is one
edge definition with a the same edge collection but a different signature
used in any other graph.

@RESTREPLYBODY{error,boolean,required,}
Flag if there was an error (true) or not (false).
It is true in this response.

@RESTREPLYBODY{code,integer,required,}
The response code.

@RESTREPLYBODY{errorNum,integer,required,}
ArangoDB error number for the error that occurred.

@RESTREPLYBODY{errorMessage,string,required,}
A message created for this error.

@EXAMPLES

Create a General Graph. This graph type does not make use of any sharding
strategy and is useful on the single server.

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

Create a SmartGraph. This graph uses 9 shards and
is sharded by the "region" attribute.
Available in the Enterprise Edition only.


@EXAMPLE_ARANGOSH_RUN{HttpGharialCreateSmart}
  var graph = require("@arangodb/general-graph");
| if (graph._exists("smartGraph")) {
|    graph._drop("smartGraph", true);
  }
  var url = "/_api/gharial";
  body = {
    name: "smartGraph",
    edgeDefinitions: [{
      collection: "edges",
      from: [ "startVertices" ],
      to: [ "endVertices" ]
    }],
    orphanCollections: [ "orphanVertices" ],
    isSmart: true,
    options: {
      replicationFactor: 2,
      numberOfShards: 9,
      smartGraphAttribute: "region"
    }
  };

  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);

  graph._drop("smartGraph", true);
@END_EXAMPLE_ARANGOSH_RUN

Create a disjoint SmartGraph. This graph uses 9 shards and
is sharded by the "region" attribute.
Available in the Enterprise Edition only.
Note that as you are using a disjoint version, you can only
create edges between vertices sharing the same region.

@EXAMPLE_ARANGOSH_RUN{HttpGharialCreateDisjointSmart}
var graph = require("@arangodb/general-graph");
| if (graph._exists("disjointSmartGraph")) {
|    graph._drop("disjointSmartGraph", true);
}
var url = "/_api/gharial";
body = {
name: "disjointSmartGraph",
edgeDefinitions: [{
collection: "edges",
from: [ "startVertices" ],
to: [ "endVertices" ]
}],
orphanCollections: [ "orphanVertices" ],
isSmart: true,
options: {
isDisjoint: true,
replicationFactor: 2,
numberOfShards: 9,
smartGraphAttribute: "region"
}
};

var response = logCurlRequest('POST', url, body);

assert(response.code === 202);

logJsonResponse(response);

graph._drop("disjointSmartGraph", true);
@END_EXAMPLE_ARANGOSH_RUN

Create a SmartGraph with a satellite vertex collection.
It uses the collection "endVertices" as a satellite collection.
This collection is cloned to all servers, all other vertex
collections are split into 9 shards
and are sharded by the "region" attribute.
Available in the Enterprise Edition only.

@EXAMPLE_ARANGOSH_RUN{HttpGharialCreateSmartWithSatellites}
var graph = require("@arangodb/general-graph");
| if (graph._exists("smartGraph")) {
|    graph._drop("smartGraph", true);
}
var url = "/_api/gharial";
body = {
name: "smartGraph",
edgeDefinitions: [{
collection: "edges",
from: [ "startVertices" ],
to: [ "endVertices" ]
}],
orphanCollections: [ "orphanVertices" ],
isSmart: true,
options: {
replicationFactor: 2,
numberOfShards: 9,
smartGraphAttribute: "region",
satellites: [ "endVertices" ]
}
};

var response = logCurlRequest('POST', url, body);

assert(response.code === 202);

logJsonResponse(response);

graph._drop("smartGraph", true);
@END_EXAMPLE_ARANGOSH_RUN

Create an EnterpriseGraph. This graph uses 9 shards,
it does not make use of specific sharding attributes.
Available in the Enterprise Edition only.

@EXAMPLE_ARANGOSH_RUN{HttpGharialCreateEnterprise}
var graph = require("@arangodb/general-graph");
| if (graph._exists("enterpriseGraph")) {
|    graph._drop("enterpriseGraph", true);
}
var url = "/_api/gharial";
body = {
name: "enterpriseGraph",
edgeDefinitions: [{
collection: "edges",
from: [ "startVertices" ],
to: [ "endVertices" ]
}],
orphanCollections: [ ],
isSmart: true,
options: {
replicationFactor: 2,
numberOfShards: 9,
}
};

var response = logCurlRequest('POST', url, body);

assert(response.code === 202);

logJsonResponse(response);

graph._drop("enterpriseGraph", true);
@END_EXAMPLE_ARANGOSH_RUN


Create a SatelliteGraph. A SatelliteGraph does not use
shards, but uses "satellite" as replicationFactor.
Make sure to keep this graph small as it is cloned
to every server.
Available in the Enterprise Edition only.

@EXAMPLE_ARANGOSH_RUN{HttpGharialCreateSatellite}
var graph = require("@arangodb/general-graph");
| if (graph._exists("satelliteGraph")) {
|    graph._drop("satelliteGraph", true);
}
var url = "/_api/gharial";
body = {
name: "satelliteGraph",
edgeDefinitions: [{
collection: "edges",
from: [ "startVertices" ],
to: [ "endVertices" ]
}],
orphanCollections: [ ],
options: {
replicationFactor: "satellite"
}
};

var response = logCurlRequest('POST', url, body);

assert(response.code === 202);

logJsonResponse(response);

graph._drop("satelliteGraph", true);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
