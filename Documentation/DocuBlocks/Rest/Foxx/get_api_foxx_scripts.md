@startDocuBlock get_api_foxx_scripts
@brief list service scripts

@RESTHEADER{GET /_api/foxx/scripts, List service scripts, listFoxxScripts}

@RESTDESCRIPTION
Fetches a list of the scripts defined by the service.

Returns an object mapping the raw script names to human-friendly names.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
