@startDocuBlock api_foxx_service_upgrade
@brief upgrade a service

@RESTHEADER{PATCH /_api/foxx/service, Upgrade service}

@RESTDESCRIPTION
Installs the given new service on top of the service currently installed at the given mount path.
This is only recommended for switching between different versions of the same service.

Unlike replacing a service, upgrading a service retains the old service's configuration
and dependencies (if any) and should therefore only be used to migrate an existing service
to a newer or equivalent service.

The request body can be any of the following formats:

- `application/zip`: a raw zip bundle containing a service
- `application/javascript`: a standalone JavaScript file
- `application/json`: a service definition as JSON
- `multipart/form-data`: a service definition as a multipart form

A service definition is an object or form with the following properties or fields:

- *configuration*: a JSON object describing configuration values
- *dependencies*: a JSON object describing dependency settings
- *source*: a fully qualified URL or an absolute path on the server's file system

When using multipart data, the *source* field can also alternatively be a file field
containing either a zip bundle or a standalone JavaScript file.

When using a standalone JavaScript file the given file will be executed
to define our service's HTTP endpoints. It is the same which would be defined
in the field `main` of the service manifest.

If *source* is a URL, the URL must be reachable from the server.
If *source* is a file system path, the path will be resolved on the server.
In either case the path or URL is expected to resolve to a zip bundle,
JavaScript file or (in case of a file system path) directory.

Note that when using file system paths in a cluster with multiple Coordinators
the file system path must resolve to equivalent files on every Coordinator.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTQUERYPARAM{teardown,boolean,optional}
Set to `true` to run the old service's teardown script.

@RESTQUERYPARAM{setup,boolean,optional}
Set to `false` to not run the new service's setup script.

@RESTQUERYPARAM{legacy,boolean,optional}
Set to `true` to install the new service in 2.8 legacy compatibility mode.

@RESTQUERYPARAM{force,boolean,optional}
Set to `true` to force service install even if no service is installed under given mount.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
