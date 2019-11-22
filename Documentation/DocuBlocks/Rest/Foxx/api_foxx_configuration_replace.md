@startDocuBlock api_foxx_configuration_replace
@brief replace configuration options

@RESTHEADER{PUT /_api/foxx/configuration, Replace configuration options}

@RESTDESCRIPTION
Replaces the given service's configuration completely.

Returns an object mapping all configuration option names to their new values.

@RESTALLBODYPARAM{data,json,required}
A JSON object mapping configuration option names to their new values.
Any omitted options will be reset to their default values or marked as unconfigured.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
