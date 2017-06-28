@startDocuBlock api_foxx_service_install
@brief install new service

@RESTHEADER{POST /_api/foxx, Install new service}

@RESTDESCRIPTION
Installs the given new service at the given mount path.

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

If *source* is a URL, the URL must be reachable from the server.
If *source* is a file system path, the path will be resolved on the server.
In either case the path or URL is expected to resolve to a zip bundle.

Note that when using file system paths in a cluster with multiple coordinators
the file system path must resolve to equivalent files on every coordinator.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path the service should be installed at.

@RESTQUERYPARAM{development,boolean,optional}
Set to `true` to enable development mode.

@RESTQUERYPARAM{setup,boolean,optional}
Set to `false` to not run the service's setup script.

@RESTQUERYPARAM{legacy,boolean,optional}
Set to `true` to install the service in 2.8 legacy compatibility mode.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the request was successful.

@endDocuBlock
