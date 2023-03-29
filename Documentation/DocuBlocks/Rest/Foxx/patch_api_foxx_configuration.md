@startDocuBlock patch_api_foxx_configuration
@brief update configuration options

@RESTHEADER{PATCH /_api/foxx/configuration, Update configuration options, updateFoxxConfiguration}

@RESTDESCRIPTION
Replaces the given service's configuration.

Returns an object mapping all configuration option names to their new values.

@RESTALLBODYPARAM{options,object,required}
A JSON object mapping configuration option names to their new values.
Any omitted options will be ignored.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
