@startDocuBlock api_foxx_readme
@brief service README

@RESTHEADER{GET /_api/foxx/readme, Service README}

@RESTDESCRIPTION
Fetches the service's README or README.md file's contents if any.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@RESTRETURNCODE{204}
Returned if no README file was found.

@endDocuBlock
