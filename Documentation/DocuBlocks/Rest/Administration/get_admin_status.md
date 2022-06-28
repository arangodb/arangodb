
@startDocuBlock get_admin_status
@brief returns Status information of the server.

@RESTHEADER{GET /_admin/status, Return status information, RestStatusHandler}

@RESTDESCRIPTION
Returns status information about the server.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Status information was returned successfully.

@RESTREPLYBODY{server,string,required,}
Always `"arango"`.

@RESTREPLYBODY{license,string,required,}
ArangoDB Edition, either `"community"` or `"enterprise"`.

@RESTREPLYBODY{version,string,required,}
The server version as a string.

@RESTREPLYBODY{mode,string,required,}
Either `"server"` or `"console"`. **Deprecated**, use `operationMode` instead.

@RESTREPLYBODY{operationMode,string,required,}
Either `"server"` or `"console"`.

@RESTREPLYBODY{foxxApi,boolean,required,}
Whether the Foxx API is enabled.

@RESTREPLYBODY{host,string,required,}
A host identifier defined by the `HOST` or `NODE_NAME` environment variable,
or a fallback value using a machine identifier or the cluster/Agency address.

@RESTREPLYBODY{hostname,string,optional,}
A hostname defined by the `HOSTNAME` environment variable.

@RESTREPLYBODY{pid,number,required,}
The process ID of _arangod_.

@RESTREPLYBODY{serverInfo,object,required,get_admin_status_server_info}
Information about the server status.

@RESTSTRUCT{progress,get_admin_status_server_info,object,required,get_admin_status_server_info_progress}
Startup and recovery information.

You can check for changes to determine whether progress was made between two
calls, but you should not rely on specific values as they may change between
ArangoDB versions. The values are only expected to change during the startup and
shutdown, i.e. while `maintenance` is `true`.

You need to start _arangod_ with the `--server.early-connections` startup option
enabled to be able to query the endpoint during the startup process.
If authentication is enabled, then you need to use the super-user JWT for the
request because the user management is not available during the startup.

@RESTSTRUCT{phase,get_admin_status_server_info_progress,string,required,}
Name of the lifecycle phase the instance is currently in. Normally one of
`"in prepare"`, `"in start"`, `"in wait"`, `"in shutdown"`, `"in stop"`,
or `"in unprepare"`.

@RESTSTRUCT{feature,get_admin_status_server_info_progress,string,required,}
Internal name of the feature that is currently being prepared, started,
stopped or unprepared.

@RESTSTRUCT{recoveryTick,get_admin_status_server_info_progress,number,required,}
Current recovery sequence number value, if the instance is currently recovering.
If the instance is already past the recovery, this attribute will contain the
last handled recovery sequence number.

@RESTSTRUCT{role,get_admin_status_server_info,string,required,}
Either `"SINGLE"`, `"COORDINATOR"`, `"PRIMARY"` (DB-Server), or `"AGENT"`.

@RESTSTRUCT{writeOpsEnabled,get_admin_status_server_info,boolean,required,}
Whether writes are enabled. **Deprecated**, use `readOnly` instead.

@RESTSTRUCT{readOnly,get_admin_status_server_info,boolean,required,}
Whether writes are disabled.

@RESTSTRUCT{maintenance,get_admin_status_server_info,boolean,required,}
Whether the maintenance mode is enabled.

@RESTSTRUCT{persistedId,get_admin_status_server_info,string,optional,}
The persisted ID, e. g. `"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"`.
*Cluster only* (Agents, Coordinators, and DB-Servers).

@RESTSTRUCT{rebootId,get_admin_status_server_info,number,optional,}
The reboot ID. Changes on every restart.
*Cluster only* (Agents, Coordinators, and DB-Servers).

@RESTSTRUCT{state,get_admin_status_server_info,string,optional,}
Either `"STARTUP"`, `"SERVING"`, or `"SHUTDOWN"`.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{address,get_admin_status_server_info,string,optional,}
The address of the server, e.g. `tcp://[::1]:8530`.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{serverId,get_admin_status_server_info,string,optional,}
The server ID, e.g. `"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"`.
*Cluster only* (Coordinators and DB-Servers).

@RESTREPLYBODY{agency,object,optional,get_admin_status_agency}
Information about the Agency.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{agencyComm,get_admin_status_agency,object,optional,get_admin_status_agency_comm}
Information about the communication with the Agency.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{endpoints,get_admin_status_agency_comm,array,optional,string}
A list of possible Agency endpoints.

@RESTREPLYBODY{coordinator,object,optional,get_admin_status_coordinator}
Information about the Coordinators.
*Cluster only* (Coordinators)

@RESTSTRUCT{foxxmaster,get_admin_status_coordinator,array,optional,string}
The server ID of the Coordinator that is the Foxx master.

@RESTSTRUCT{isFoxxmaster,get_admin_status_coordinator,array,optional,string}
Whether the queried Coordinator is the Foxx master.

@RESTREPLYBODY{agent,object,optional,get_admin_status_agent}
Information about the Agents.
*Cluster only* (Agents)

@RESTSTRUCT{id,get_admin_status_agent,string,optional,}
Server ID of the queried Agent.

@RESTSTRUCT{leaderId,get_admin_status_agent,string,optional,}
Server ID of the leading Agent.

@RESTSTRUCT{leading,get_admin_status_agent,boolean,optional,}
Whether the queried Agent is the leader.

@RESTSTRUCT{endpoint,get_admin_status_agent,string,optional,}
The endpoint of the queried Agent.

@RESTSTRUCT{term,get_admin_status_agent,number,optional,}
The current term number.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAdminStatus_cluster}
    var url = "/_admin/status";
    var response = logCurlRequest("GET", url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
