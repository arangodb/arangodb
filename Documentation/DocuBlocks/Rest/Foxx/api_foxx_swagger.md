@startDocuBlock api_foxx_swagger
@brief swagger description

@RESTHEADER{GET /_api/foxx/swagger, Swagger description}

@RESTDESCRIPTION
Fetches the Swagger API description for the service at the given mount path.

The response body will be an OpenAPI 2.0 compatible JSON description of the service API.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
