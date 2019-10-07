@startDocuBlock api_foxx_dependencies_get
@brief get dependency options

@RESTHEADER{GET /_api/foxx/dependencies, Get dependency options}

@RESTDESCRIPTION
Fetches the current dependencies for service at the given mount path.

Returns an object mapping the dependency names to their definitions
including a human-friendly *title* and the *current* mount path (if any).

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
