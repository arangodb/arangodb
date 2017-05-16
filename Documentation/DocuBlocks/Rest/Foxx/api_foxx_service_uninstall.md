@startDocuBlock api_foxx_service_uninstall
@brief uninstall service

@RESTHEADER{DELETE /_api/foxx/service, Uninstall service}

@RESTDESCRIPTION
Removes the service at the given mount path from the database and file system.

Returns an empty response on success.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTQUERYPARAM{teardown,boolean,optional}
Set to `false` to not run the service's teardown script.

@RESTRETURNCODES

@RESTRETURNCODE{204}
Returned if the request was successful.

@endDocuBlock
