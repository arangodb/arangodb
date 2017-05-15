@startDocuBlock api_foxx_service_details
@brief service metadata

@RESTHEADER{GET /_api/foxx/service, Service description}

@RESTDESCRIPTION
Fetches detailed information for the service at the given mount path.

Returns an object with the following attributes:

- *mount*: the mount path of the service
- *path*: the local file system path of the service
- *development*: *true* if the service is running in development mode
- *legacy*: *true* if the service is running in 2.8 legacy compatibility mode
- *manifest*: the normalized JSON manifest of the service

Additionally the object may contain the following attributes if they have been set on the manifest:

- *name*: a string identifying the service type
- *version*: a semver-compatible version string

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@RESTRETURNCODE{400}
Returned if the mount path is unknown.

@endDocuBlock
