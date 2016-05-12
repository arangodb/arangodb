
@startDocuBlock JSF_cluster_test_GET
@brief executes a cluster roundtrip for sharding

@RESTHEADER{GET /_admin/cluster-test, Execute cluster roundtrip}

@RESTDESCRIPTION

Executes a cluster roundtrip from a coordinator to a DB server and
back. This call only works in a coordinator node in a cluster.
One can and should append an arbitrary path to the URL and the
part after */_admin/cluster-test* is used as the path of the HTTP
request which is sent from the coordinator to a DB node. Likewise,
any form data appended to the URL is forwarded in the request to the
DB node. This handler takes care of all request types (see below)
and uses the same request type in its request to the DB node.

The following HTTP headers are interpreted in a special way:

  - *X-Shard-ID*: This specifies the ID of the shard to which the
    cluster request is sent and thus tells the system to which DB server
    to send the cluster request. Note that the mapping from the
    shard ID to the responsible server has to be defined in the
    agency under *Current/ShardLocation/<shardID>*. One has to give
    this header, otherwise the system does not know where to send
    the request.
  - *X-Client-Transaction-ID*: the value of this header is taken
    as the client transaction ID for the request
  - *X-Timeout*: specifies a timeout in seconds for the cluster
    operation. If the answer does not arrive within the specified
    timeout, an corresponding error is returned and any subsequent
    real answer is ignored. The default if not given is 24 hours.
  - *X-Synchronous-Mode*: If set to *true* the test function uses
    synchronous mode, otherwise the default asynchronous operation
    mode is used. This is mainly for debugging purposes.
  - *Host*: This header is ignored and not forwarded to the DB server.
  - *User-Agent*: This header is ignored and not forwarded to the DB
    server.

All other HTTP headers and the body of the request (if present, see
other HTTP methods below) are forwarded as given in the original request.

In asynchronous mode the DB server answers with an HTTP request of its
own, in synchronous mode it sends a HTTP response. In both cases the
headers and the body are used to produce the HTTP response of this
API call.

@RESTRETURNCODES

The return code can be anything the cluster request returns, as well as:

@RESTRETURNCODE{200}
is returned when everything went well, or if a timeout occurred. In the
latter case a body of type application/json indicating the timeout
is returned.

@RESTRETURNCODE{403}
is returned if ArangoDB is not running in cluster mode.

@RESTRETURNCODE{404}
is returned if ArangoDB was not compiled for cluster operation.
@endDocuBlock

