@startDocuBlock put_api_foxx_dependencies
@brief replace dependencies options

@RESTHEADER{PUT /_api/foxx/dependencies, Replace dependencies options, replaceFoxxDependencies}

@RESTDESCRIPTION
Replaces the given service's dependencies completely.

Returns an object mapping all dependency names to their new mount paths.

@RESTALLBODYPARAM{options,object,required}
A JSON object mapping dependency names to their new mount paths.
Any omitted dependencies will be disabled.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
