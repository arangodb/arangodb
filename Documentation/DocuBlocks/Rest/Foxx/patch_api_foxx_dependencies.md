@startDocuBlock patch_api_foxx_dependencies

@RESTHEADER{PATCH /_api/foxx/dependencies, Update the dependency options, updateFoxxDependencies}

@RESTDESCRIPTION
Replaces the given service's dependencies.

Returns an object mapping all dependency names to their new mount paths.

@RESTALLBODYPARAM{options,object,required}
A JSON object mapping dependency names to their new mount paths.
Any omitted dependencies will be ignored.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
