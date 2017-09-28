HTTP Interface for Endpoints
============================

The API `/_api/endpoint` is *deprecated*. For cluster mode there
is `/_api/cluster/endpoints` to find all current coordinator endpoints
(see below).

The ArangoDB server can listen for incoming requests on multiple *endpoints*.

The endpoints are normally specified either in ArangoDB's configuration
file or on the command-line, using the "--server.endpoint" option.
The default endpoint for ArangoDB is *tcp://127.0.0.1:8529* or
*tcp://localhost:8529*.

Please note that all endpoint management operations can only be accessed via
the default database (*_system*) and none of the other databases.

Asking about Endpoints via HTTP
---------------------------

<!-- js/actions/api-cluster.js -->
@startDocuBlock JSF_get_api_cluster_endpoints

<!-- arangod/RestHandler/RestEndpointHandler.h -->
@startDocuBlock JSF_get_api_endpoint
