
@startDocuBlock put_cluster_maintenance
@brief Set or unset the cluster supervision (agency) maintenance mode

@RESTHEADER{PUT /_admin/cluster/maintenance, Enable or disable the supervision maintenance mode}

@RESTDESCRIPTION
This API allows you to temporarily enable the supervision maintenance mode. Be aware that no 
automatic failovers of any kind will take place while the maintenance mode is enabled.
The _cluster_ supervision reactivates automatically _60 minutes_ after disabling it.

To enale the mode the request body must contain the string `"on"`, to disable it send the string
`"off"` (Please note it _must_ be lowercase as well as include the quotes).

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} if the request contained an invalid body

@RESTRETURNCODE{501} if the request was send to a node other than a coordinator or single-server

@RESTRETURNCODE{504} if the request timed out while enabling the maintenance mode

@endDocuBlock
