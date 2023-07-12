@startDocuBlock put_api_foxx_configuration

@RESTHEADER{PUT /_api/foxx/configuration, Replace the configuration options, replaceFoxxConfiguration}

@RESTDESCRIPTION
Replaces the given service's configuration completely.

Returns an object mapping all configuration option names to their new values.

@RESTALLBODYPARAM{options,object,required}
A JSON object mapping configuration option names to their new values.
Any omitted options will be reset to their default values or marked as unconfigured.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
