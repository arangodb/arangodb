
@startDocuBlock put_admin_cluster_maintenance_dbserver

@RESTHEADER{PUT /_admin/cluster/maintenance/{DB-Server-ID}, Set the DB-Server maintenance mode, setDbserverMaintenance}

@RESTDESCRIPTION
Enable or disable the maintenance mode of a DB-Server.

For rolling upgrades or rolling restarts, DB-Servers can be put into
maintenance mode, so that no attempts are made to re-distribute the data in a
cluster for such planned events. DB-Servers in maintenance mode are not
considered viable failover targets because they are likely restarted soon.

@RESTURLPARAMETERS

@RESTURLPARAM{DB-Server-ID,string,required}
The ID of a DB-Server.

@RESTBODYPARAM{mode,string,required,}
The mode to put the DB-Server in. Possible values:
- `"maintenance"`
- `"normal"`

@RESTBODYPARAM{timeout,integer,optional,}
After how many seconds the maintenance mode shall automatically end.
You can send another request when the DB-Server is already in maintenance mode
to extend the timeout.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The request was successful.

@RESTREPLYBODY{error,boolean,required,}
Whether an error occurred. `false` in this case.

@RESTREPLYBODY{code,integer,required,}
The status code. `200` in this case.

@RESTRETURNCODE{400}
if the request contained an invalid body

@RESTRETURNCODE{412}
if the request was sent to an Agency node

@RESTRETURNCODE{504}
if the request timed out while enabling the maintenance mode

@endDocuBlock
