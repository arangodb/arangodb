
@startDocuBlock get_admin_status
@brief returns status information of the server.

@RESTHEADER{GET /_admin/status, Return status information, RestStatusHandler}

@RESTDESCRIPTION
Returns status information about the server.

This is intended for manual use by the support and should
never be used for monitoring or automatic tests. The results
are subject to change without notice.

<!-- TODO -->

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
The hostname.

<!-- TODO: see *ServerState*. -->

@RESTREPLYBODY{pid,number,required,}
The process ID of _arangod_.

@RESTREPLYBODY{serverInfo,object,required,get_admin_status_server_info}
Information about the server status.

@RESTSTRUCT{progress,get_admin_status_server_info,object,required,get_admin_status_server_info_progress}

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

@RESTSTRUCT{persistedId,get_admin_status_server_info,string,required,}
The persisted ID, e. g. `"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"`.
*Cluster only* (Agents, Coordinators, and DB-Servers).

@RESTSTRUCT{rebootId,get_admin_status_server_info,number,required,}
The reboot ID.
*Cluster only* (Agents, Coordinators, and DB-Servers).

@RESTSTRUCT{state,get_admin_status_server_info,string,required,}
Always `"SERVING"`.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{address,get_admin_status_server_info,string,required,}
The address of the server, e.g. `tcp://[::1]:8530`.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{serverId,get_admin_status_server_info,string,required,}
The server ID, e.g. `"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"`.
*Cluster only* (Coordinators and DB-Servers).

@RESTREPLYBODY{agency,object,required,get_admin_status_agency}
Information about the Agency.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{agencyComm,get_admin_status_agency,object,required,get_admin_status_agency_comm}
Information about the communication with the Agency.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{endpoints,get_admin_status_agency_comm,array,required,string}
A list of possible Agency endpoints.

@RESTREPLYBODY{coordinator,object,required,get_admin_status_coordinator}
Information about the Coordinators.
*Cluster only* (Coordinators)

@RESTSTRUCT{foxxmaster,get_admin_status_coordinator,array,required,string}
The server ID of the Coordinator that is the Foxx master.

@RESTSTRUCT{isFoxxmaster,get_admin_status_coordinator,array,required,string}
Whether the queried Coordinator is the Foxx master.

@RESTREPLYBODY{agent,object,required,get_admin_status_agent}
Information about the Agents.
*Cluster only* (Agents)

@RESTSTRUCT{id,get_admin_status_agent,string,required,}
Server ID of the queried Agent.

@RESTSTRUCT{leaderId,get_admin_status_agent,string,required,}
Server ID of the leading Agent.

@RESTSTRUCT{leading,get_admin_status_agent,boolean,required,}
Whether the queried Agent is the leader.

@RESTSTRUCT{endpoint,get_admin_status_agent,string,required,}
The endpoint of the queried Agent.

@RESTSTRUCT{term,get_admin_status_agent,number,required,}
The current term number.

@endDocuBlock
