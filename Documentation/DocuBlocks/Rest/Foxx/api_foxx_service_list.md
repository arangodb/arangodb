@startDocuBlock api_foxx_service_list
@brief list installed services

@RESTHEADER{GET /_api/foxx, List installed services}

@RESTDESCRIPTION
Fetches a list of services installed in the current database.

Returns a list of objects with the following attributes:

- *mount*: the mount path of the service
- *development*: *true* if the service is running in development mode
- *legacy*: *true* if the service is running in 2.8 legacy compatibility mode
- *provides*: the service manifest's *provides* value or an empty object

Additionally the object may contain the following attributes if they have been set on the manifest:

- *name*: a string identifying the service type
- *version*: a semver-compatible version string

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{excludeSystem,boolean,optional}
Whether or not system services should be excluded from the result.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
