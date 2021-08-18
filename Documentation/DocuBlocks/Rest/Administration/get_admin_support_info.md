@startDocuBlock get_admin_support_info
@brief Get deployment information

@RESTHEADER{GET /_admin/support-info, Get information about the deployment, getSupportInfo}

@RESTDESCRIPTION
Retrieves deployment information for support purposes. The endpoint returns data
about the ArangoDB version used, the host (operating system, server ID, CPU and
storage capacity, current utilization, a few metrics) and the other servers in
the deployment (in case of Active Failover or cluster deployments).

As this API may reveal sensitive data about the deployment, it can only be 
accessed from inside the `_system` database. In addition, there is a policy
control startup option `--server.support-info-api` that controls if and to whom
the API is made available.

@RESTRETURNCODES

@RESTRETURNCODE{200}

@RESTREPLYBODY{date,string,required,}
ISO 8601 datetime string of when the information was requested.

@RESTREPLYBODY{deployment,object,required,}
An object with at least a `type` attribute, indicating the deployment type.

In case of a `"single"` server, additional information is provided in the
top-level `host` attribute.

In case of a `"cluster"`, there is a `servers` object that contains a nested
object for each Coordinator and DB-Server, using the server ID as key. Each
object holds information about the ArangoDB instance as well as the host machine.
There are additional attributes for the number of `agents`, `coordinators`,
`dbServers`, and `shards`.

@RESTREPLYBODY{host,object,optional,}
An object that holds information about the ArangoDB instance as well as the
host machine. Only set in case of single servers.

@RESTRETURNCODE{404}
The support info API is turned off.

@EXAMPLES

Query support information from a single server

@EXAMPLE_ARANGOSH_RUN{RestAdminSupportInfo}
    var url = "/_admin/support-info";
    var response = logCurlRequest("GET", url);
    assert(response.code === 200);
    assert(JSON.parse(response.body).host !== undefined);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Query support information from a cluster

@EXAMPLE_ARANGOSH_RUN{RestAdminSupportInfo_cluster}
    var url = "/_admin/support-info";
    var response = logCurlRequest("GET", url);
    assert(response.code === 200);
    assert(JSON.parse(response.body).deployment.servers !== undefined);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
