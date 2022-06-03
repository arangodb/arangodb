
@startDocuBlock JSF_get_admin_status
@brief returns status information of the server.

@RESTHEADER{GET /_admin/status, Return status information, RestStatusHandler}

@RESTDESCRIPTION
Returns status information about the server.

This is intended for manual use by the support and should
never be used for monitoring or automatic tests. The results
are subject to change without notice.

TODO

@RESTRETURNCODES

@RESTRETURNCODE{200}
Status information was returned successfully.

@RESTREPLYBODY{server,string,required,}
Always `"arango"`.

@RESTREPLYBODY{license,string,required,}
Either `"community"` or `"enterprise"`.

@RESTREPLYBODY{version,string,required,}
The server version as a string.

@RESTREPLYBODY{mode,string,required,}
Either `"server"` or `"console"`.

@RESTREPLYBODY{host,string,required,}
The hostname.

TODO: see *ServerState*.

@RESTREPLYBODY{serverInfo,object,required,get_admin_status_server_info}
Information about the server status.

@RESTSTRUCT{role,get_admin_status_server_info,string,required,}
Either `"SINGLE"`, `"COORDINATOR"`, `"PRIMARY"` (DB-Server), or `"AGENT"`.

@RESTSTRUCT{writeOpsEnabled,get_admin_status_server_info,boolean,required,}
Whether writes are enabled.

@RESTSTRUCT{maintenance,get_admin_status_server_info,boolean,required,}
Whether the maintenance mode is enabled.

@RESTSTRUCT{persistedId,get_admin_status_server_info,string,required,}
The persisted ID, e. g. `"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"`.
*Cluster only* (Agents, Coordinators, and DB-Servers).

@RESTSTRUCT{state,get_admin_status_server_info,string,required,}
Always `"SERVING"`.
*Cluster only* (Coordinators and DB-Servers).

TODO: true?

@RESTSTRUCT{address,get_admin_status_server_info,string,required,}
The address of the server, e.g. `tcp://[::1]:8530`.
*Cluster only* (Coordinators and DB-Servers).

@RESTSTRUCT{serverId,get_admin_status_server_info,string,required,}
The server ID, e.g. `"CRDN-e427b441-5087-4a9a-9983-2fb1682f3e2a"`.
*Cluster only* (Coordinators and DB-Servers).

@RESTREPLYBODY{agency,object,required,get_admin_status_agency}
TODO

@RESTSTRUCT{endpoints,get_admin_status_agency,array,required,string}
A list of possible Agency endpoints.

@RESTREPLYBODY{coordinator,object,required,get_admin_status_coordinator}
Information that only Coordinators will have.

@RESTSTRUCT{foxxmaster,get_admin_status_coordinator,array,required,string}
The server ID of the Foxx master.

@RESTSTRUCT{isFoxxmaster,get_admin_status_coordinator,array,required,string}
Whether the asked server is the Foxx master.

@RESTREPLYBODY{agent,object,required,get_admin_status_agent}
Information that only Agents will have.

@RESTSTRUCT{id,get_admin_status_agent,string,required,}
Server ID of this Agent.

@RESTSTRUCT{leaderId,get_admin_status_agent,string,required,}
Server ID of the leader.

@RESTSTRUCT{leading,get_admin_status_agent,boolean,required,}
Whether the asked Agent is the leader.

@RESTSTRUCT{endpoint,get_admin_status_agent,string,required,}
The endpoint of this Agent.

@RESTSTRUCT{term,get_admin_status_agent,number,required,}
The current term number.

@endDocuBlock
