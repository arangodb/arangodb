
@startDocuBlock put_cluster_maintenance
@brief Enable or disable the cluster supervision (Agency) maintenance mode

@RESTHEADER{PUT /_admin/cluster/maintenance, Enable or disable the supervision maintenance mode}

@RESTDESCRIPTION
This API allows you to temporarily enable the supervision maintenance mode. Be aware that no
automatic failovers of any kind will take place while the maintenance mode is enabled.
The _cluster_ supervision reactivates itself automatically _60 minutes_ after disabling it.

To enable the maintenance mode the request body must contain the string `"on"`. To disable it, send the string
`"off"` (Please note it _must_ be lowercase as well as include the quotes).

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} if the request contained an invalid body

@RESTRETURNCODE{501} if the request was sent to a node other than a Coordinator or single-server

@RESTRETURNCODE{504} if the request timed out while enabling the maintenance mode

@endDocuBlock
