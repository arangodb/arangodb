
@startDocuBlock get_admin_server_role
@brief Return the role of a server in a cluster

@RESTHEADER{GET /_admin/server/role, Return the role of a server in a cluster, handleRole}

@RESTDESCRIPTION
Returns the role of a server in a cluster.
The role is returned in the *role* attribute of the result.
Possible return values for *role* are:
- *SINGLE*: the server is a standalone server without clustering
- *COORDINATOR*: the server is a Coordinator in a cluster
- *PRIMARY*: the server is a DB-Server in a cluster
- *SECONDARY*: this role is not used anymore
- *AGENT*: the server is an Agency node in a cluster
- *UNDEFINED*: in a cluster, *UNDEFINED* is returned if the server role cannot be
   determined.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned in all cases.

@RESTREPLYBODY{error,boolean,required,}
always *false*

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code, always 200

@RESTREPLYBODY{errorNum,integer,required,int64}
the server error number

@RESTREPLYBODY{role,string,required,string}
one of [ *SINGLE*, *COORDINATOR*, *PRIMARY*, *SECONDARY*, *AGENT*, *UNDEFINED*]

@endDocuBlock
