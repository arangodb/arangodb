@startDocuBlock api_foxx_bundle
@brief download service bundle

@RESTHEADER{POST /_api/foxx/download, Download service bundle}

@RESTDESCRIPTION
Downloads a zip bundle of the service directory.

When development mode is enabled, this always creates a new bundle.

Otherwise the bundle will represent the version of a service that
is installed on that ArangoDB instance.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@RESTRETURNCODE{400}
Returned if the mount path is unknown.

@endDocuBlock
