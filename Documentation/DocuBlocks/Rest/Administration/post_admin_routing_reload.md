
@startDocuBlock post_admin_routing_reload

@RESTHEADER{POST /_admin/routing/reload, Reload the routing table, reloadRouting}

@RESTDESCRIPTION
Reloads the routing information from the `_routing` system collection if it
exists, and makes Foxx rebuild its local routing table on the next request.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The routing information has been reloaded successfully.

@endDocuBlock
