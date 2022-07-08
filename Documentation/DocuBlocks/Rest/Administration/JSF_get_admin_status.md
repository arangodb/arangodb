
@startDocuBlock JSF_get_admin_status
@brief returns status information of the server.

@RESTHEADER{GET /_admin/status, Return status information, RestStatusHandler}

@RESTDESCRIPTION
Returns status information about the server.

This is intended for manual use by the support and should
never be used for monitoring or automatic tests. The results
are subject to change without notice.

The call returns an object with the following attributes:

- *server*: always *arango*.

- *license*: either *community* or *enterprise*.

- *version*: the server version as string.

- *mode* : either *server* or *console*.

- *host*: the hostname, see *ServerState*.

- *serverInfo.role*: either *SINGLE*, *COORDINATOR*, *PRIMARY*, *AGENT*.

- *serverInfo.writeOpsEnabled*: boolean, true if writes are enabled.

- *serverInfo.maintenance*: boolean, true if maintenance mode is enabled.

- *agency.endpoints*: a list of possible Agency endpoints.

An Agent, Coordinator or primary will also have

- *serverInfo.persistedId*: the persisted ide, e. g. *"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"*.

A Coordinator or primary will also have

- *serverInfo.state*: *SERVING*

- *serverInfo.address*: the address of the server, e. g. *tcp://[::1]:8530*.

- *serverInfo.serverId*: the server ide, e. g. *"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"*.

A Coordinator will also have

- *coordinator.foxxmaster*: the server id of the foxx master.

- *coordinator.isFoxxmaster*: boolean, true if the server is the foxx master.

An Agent will also have

- *agent.id*: server id of this Agent.

- *agent.leaderId*: server id of the leader.

- *agent.leading*: boolean, true if leading.

- *agent.endpoint*: the endpoint of this Agent.

- *agent.term*: current term number.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Status information was returned successfully.

@endDocuBlock
