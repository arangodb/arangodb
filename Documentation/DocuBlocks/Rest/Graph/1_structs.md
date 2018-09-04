@RESTSTRUCT{name,graph_representation,string,required,}
The name of the graph.

@RESTSTRUCT{edgeDefinitions,graph_representation,array,required,post_api_gharial_create_edge_defs}
An array of definitions for the relations of the graph.
Each has the following type:

@RESTSTRUCT{orphanCollections,graph_representation,array,required,string}
An array of additional vertex collections.
Documents within these collections do not have edges within this graph.

@RESTSTRUCT{numberOfShards,graph_representation,integer,required,}
Number of shards created for every new collection in the graph.

@RESTSTRUCT{replicationFactor,graph_representation,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{_id,graph_representation,string,required,}
The internal id value of this graph. 

@RESTSTRUCT{_rev,graph_representation,string,required,}
The revision of this graph. Can be used to make sure to not override
concurrent modifications to this graph.

@RESTSTRUCT{replicationFactor,graph_representation,integer,required,}
The replication factor used for every new collection in the graph.

@RESTSTRUCT{isSmart,graph_representation,boolean,required,}
Flag if the graph is a SmartGraph (Enterprise only) or not.

@RESTSTRUCT{smartGraphAttribute,graph_representation,string,optional,}
The name of the sharding attribute in smart graph case (Enterprise Only)
