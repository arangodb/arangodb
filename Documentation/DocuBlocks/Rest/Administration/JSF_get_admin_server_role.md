
@startDocuBlock JSF_get_admin_server_role
@brief Get to know whether this server is a Coordinator or DB-Server

@RESTHEADER{GET /_admin/server/role, Return role of a server in a cluster}

@RESTDESCRIPTION

Returns the role of a server in a cluster.
The role is returned in the *role* attribute of the result.
Possible return values for *role* are:
- *COORDINATOR*: the server is a coordinator in a cluster
- *PRIMARY*: the server is a primary database server in a cluster
- *SECONDARY*: the server is a secondary database server in a cluster
- *UNDEFINED*: in a cluster, *UNDEFINED* is returned if the server role cannot be
   determined. On a single server, *UNDEFINED* is the only possible return
   value.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned in all cases.
@endDocuBlock

