@startDocuBlock api_foxx_development_disable
@brief disable development mode

@RESTHEADER{DELETE /_api/foxx/development, Disable development mode}

@RESTDESCRIPTION
Puts the service at the given mount path into production mode.

When running ArangoDB in a cluster with multiple coordinators this will
replace the service on all other coordinators with the version on this
coordinator.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
