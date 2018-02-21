
@startDocuBlock patch_cluster_test
@brief executes a cluster roundtrip for sharding

@RESTHEADER{PATCH /_admin/cluster-test, Update cluster roundtrip}

@RESTALLBODYPARAM{body,object,required}

@RESTDESCRIPTION
See GET method. The body can be any type and is simply forwarded.

@RESTRETURNCODE{200}
is returned when everything went well, or if a timeout occurred. In the
latter case a body of type application/json indicating the timeout
is returned.

@RESTRETURNCODE{403}
is returned if ArangoDB is not running in cluster mode.

@RESTRETURNCODE{404}
is returned if ArangoDB was not compiled for cluster operation.
@endDocuBlock

