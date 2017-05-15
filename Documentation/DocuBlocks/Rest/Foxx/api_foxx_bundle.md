@startDocuBlock api_foxx_bundle
@brief download service bundle

@RESTHEADER{GET /_api/foxx/bundle, Download service bundle}

@RESTDESCRIPTION
Downloads a zip bundle of the service directory.

When development mode is enabled, this always creates a new bundle.

Otherwise the bundle will represent the version of a service that
is installed on that ArangoDB instance.

If a bundle is available, the *ETag* response header can be used
to identify the version of the service over time.

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{If-Match,string,optional}
Only return bundle matching the given ETag.

@RESTHEADERPARAM{If-None-Match,string,optional}
Only return bundle not matching the given ETag.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was sucessful.

@RESTRETURNCODE{304}
No matching bundle available if *if-none-match* is set.

@RESTRETURNCODE{404}
Bundle not available or no matching bundle available if *if-match* is set

@endDocuBlock
