
@startDocuBlock get_admin_cluster_maintenance_dbserver

@RESTHEADER{GET /_admin/cluster/maintenance/{DB-Server-ID}, Get the maintenance status of a DB-Server, getDbserverMaintenance}

@RESTDESCRIPTION
Check whether the specified DB-Server is in maintenance mode and until when.

@RESTURLPARAMETERS

@RESTURLPARAM{DB-Server-ID,string,required}
The ID of a DB-Server.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The request was successful.

@RESTREPLYBODY{error,boolean,required,}
Whether an error occurred. `false` in this case.

@RESTREPLYBODY{code,integer,required,}
The status code. `200` in this case.

@RESTREPLYBODY{result,object,optional,get_cluster_maintenance_dbserver_result}
The result object with the status. This attribute is omitted if the DB-Server
is in normal mode.

@RESTSTRUCT{Mode,get_cluster_maintenance_dbserver_result,string,required,}
The mode of the DB-Server. The value is `"maintenance"`.

@RESTSTRUCT{Until,get_cluster_maintenance_dbserver_result,string,required,dateTime}
Until what date and time the maintenance mode currently lasts, in the
ISO 8601 date/time format.

@RESTRETURNCODE{400}
if the request contained an invalid body

@RESTRETURNCODE{412}
if the request was sent to an Agent node

@RESTRETURNCODE{504}
if the request timed out while enabling the maintenance mode

@endDocuBlock
