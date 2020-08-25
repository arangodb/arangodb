@RESTSTRUCT{name,graph_representation,string,required,}
The name of the graph.

@RESTSTRUCT{edgeDefinitions,graph_representation,array,required,graph_edge_definition}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation,array,required,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation,string,required,}
The internal id value of this graph.

@RESTSTRUCT{_rev,graph_representation,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation,integer,required,}
The replication factor used for every new collection in the graph.
Can also be the string `"satellite"` for a SatelliteGraph
(Enterprise Edition only).

@RESTSTRUCT{writeConcern,graph_representation,integer,optional,}
Default write concern for new collections in the graph.
It determines how many copies of each shard are required to be
in sync on the different DB-Servers. If there are less then these many copies
in the cluster a shard will refuse to write. Writes to shards with enough
up-to-date copies will succeed at the same time however. The value of
*writeConcern* can not be larger than *replicationFactor*. _(cluster only)_

@RESTSTRUCT{isSmart,graph_representation,boolean,required,}
Whether the graph is a SmartGraph (Enterprise Edition only).

@RESTSTRUCT{isDisjoint,graph_representation,boolean,required,}
Whether the graph is a Disjoint SmartGraph (Enterprise Edition only).

@RESTSTRUCT{smartGraphAttribute,graph_representation,string,optional,}
Name of the sharding attribute in the SmartGraph case (Enterprise Edition only).

@RESTSTRUCT{isSatellite,graph_representation,boolean,required,}
Flag if the graph is a SatelliteGraph (Enterprise Edition only) or not.

@RESTSTRUCT{_id,vertex_representation,string,required,}
The _id value of the stored data.

@RESTSTRUCT{_key,vertex_representation,string,required,}
The _key value of the stored data.

@RESTSTRUCT{_rev,vertex_representation,string,required,}
The _rev value of the stored data.

@RESTSTRUCT{_id,edge_representation,string,required,}
The _id value of the stored data.

@RESTSTRUCT{_key,edge_representation,string,required,}
The _key value of the stored data.

@RESTSTRUCT{_rev,edge_representation,string,required,}
The _rev value of the stored data.

@RESTSTRUCT{_from,edge_representation,string,required,}
The _from value of the stored data.

@RESTSTRUCT{_to,edge_representation,string,required,}
The _to value of the stored data.

@RESTSTRUCT{collection,graph_edge_definition,string,required,}
Name of the edge collection, where the edge are stored in.

@RESTSTRUCT{from,graph_edge_definition,array,required,string}
List of vertex collection names.
Edges in collection can only be inserted if their _from is in any of the collections here.

@RESTSTRUCT{to,graph_edge_definition,array,required,string}
List of vertex collection names.
Edges in collection can only be inserted if their _to is in any of the collections here.
