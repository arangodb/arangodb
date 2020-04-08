
@startDocuBlock get_api_replication_cluster_inventory
@brief returs an overview of collections and indexes in a cluster

@RESTHEADER{GET /_api/replication/clusterInventory, Return cluster inventory of collections and indexes, handleCommandClusterInventory}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{includeSystem,boolean,optional}
Include system collections in the result. The default value is *true*.

@RESTDESCRIPTION
Returns the array of collections and indexes available on the cluster.

The response will be an array of JSON objects, one for each collection.
Each collection containscontains exactly two keys "parameters" and
"indexes". This
information comes from Plan/Collections/{DB-Name}/* in the Agency,
just that the *indexes* attribute there is relocated to adjust it to
the data format of arangodump.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request was executed successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if an error occurred while assembling the response.
@endDocuBlock
