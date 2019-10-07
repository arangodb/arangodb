@startDocuBlock api_foxx_dependencies_replace
@brief replace dependencies options

@RESTHEADER{PUT /_api/foxx/dependencies, Replace dependencies options}

@RESTDESCRIPTION
Replaces the given service's dependencies completely.

Returns an object mapping all dependency names to their new mount paths.

@RESTALLBODYPARAM{data,json,required}
A JSON object mapping dependency names to their new mount paths.
Any omitted dependencies will be disabled.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
