
@startDocuBlock put_cluster_maintenance
@brief Enable or disable the cluster supervision (Agency) maintenance mode

@RESTHEADER{PUT /_admin/cluster/maintenance, Enable or disable the supervision maintenance mode}

@RESTDESCRIPTION
This API allows to temporarily enable the supervision maintenance mode. Please be aware that no
automatic failovers of any kind will take place while the maintenance mode is enabled.
The cluster supervision reactivates itself automatically at some point after disabling it.

To enable the maintenance mode the request body must contain the string `"on"`
(Please note it _must_ be lowercase as well as include the quotes). This will enable the
maintenance mode for 60 minutes, i.e. the supervision maintenance will reactivate itself
after 60 minutes.

To enable the maintenance mode for a different duration than 60 minutes, it is possible to send
the desired duration value (in seconds) as the request body. For example, sending `"7200"`
(including the quotes) will enable the maintenance mode for 7200 seconds, i.e. 2 hours.

To disable the maintenance mode the request body must contain the string `"off"` 
(Please note it _must_ be lowercase as well as include the quotes).

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} if the request contained an invalid body

@RESTRETURNCODE{501} if the request was sent to a node other than a Coordinator or single-server

@RESTRETURNCODE{504} if the request timed out while enabling the maintenance mode

@endDocuBlock
