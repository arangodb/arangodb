@startDocuBlock api_foxx_development_enable
@brief enable development mode

@RESTHEADER{POST /_api/foxx/development, Enable development mode}

@RESTDESCRIPTION
Puts the service into development mode.

While the service is running in development mode the service will be reloaded
from the filesystem and its setup script (if any) will be re-executed every
time the service handles a request.

When running ArangoDB in a cluster with multiple Coordinators note that changes
to the filesystem on one Coordinator will not be reflected across the other
Coordinators. This means you should treat your Coordinators as inconsistent
as long as any service is running in development mode.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
