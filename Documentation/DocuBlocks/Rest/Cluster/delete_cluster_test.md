
@startDocuBlock delete_cluster_test
@brief executes a cluster roundtrip for sharding

@RESTHEADER{DELETE /_admin/cluster-test, Delete cluster roundtrip}

@RESTDESCRIPTION
See GET method.

@RESTRETURNCODE{200}
is returned when everything went well, or if a timeout occurred. In the
latter case a body of type application/json indicating the timeout
is returned.

@RESTRETURNCODE{403}
is returned if ArangoDB is not running in cluster mode.

@RESTRETURNCODE{404}
is returned if ArangoDB was not compiled for cluster operation.

@endDocuBlock

