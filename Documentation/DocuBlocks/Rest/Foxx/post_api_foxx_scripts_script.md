@startDocuBlock post_api_foxx_scripts_script

@RESTHEADER{POST /_api/foxx/scripts/{name}, Run a service script, runFoxxScript}

@RESTALLBODYPARAM{data,json,optional}
An arbitrary JSON value that will be parsed and passed to the
script as its first argument.

@RESTURLPARAMETERS

@RESTURLPARAM{name,string,required}
Name of the script to run.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTDESCRIPTION
Runs the given script for the service at the given mount path.

Returns the exports of the script, if any.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
