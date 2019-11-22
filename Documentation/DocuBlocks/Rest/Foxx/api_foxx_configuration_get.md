@startDocuBlock api_foxx_configuration_get
@brief get configuration options

@RESTHEADER{GET /_api/foxx/configuration, Get configuration options}

@RESTDESCRIPTION
Fetches the current configuration for the service at the given mount path.

Returns an object mapping the configuration option names to their definitions
including a human-friendly *title* and the *current* value (if any).

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
