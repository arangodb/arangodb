@startDocuBlock api_foxx_scripts_run
@brief run service script

@RESTHEADER{POST /_api/foxx/scripts/{name}, Run service script}

@RESTDESCRIPTION
Runs the given script for the service at the given mount path.

Returns the exports of the script, if any.

@RESTALLBODYPARAM{data,json,optional}
An arbitrary JSON value that will be parsed and passed to the
script as its first argument.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{name,string,required}
Name of the script to run.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was sucessful.

@endDocuBlock
