
@startDocuBlock JSF_get_api_aqlfunction
@brief gets all reqistered AQL user functions

@RESTHEADER{GET /_api/aqlfunction, Return registered AQL user functions}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{namespace,string,optional}
Returns all registered AQL user functions from namespace *namespace*.

@RESTDESCRIPTION
Returns all registered AQL user functions.

The call will return a JSON array with all user functions found. Each user
function will at least have the following attributes:

- *name*: The fully qualified name of the user function

- *code*: A string representation of the function body

@RESTRETURNCODES

@RESTRETURNCODE{200}
if success *HTTP 200* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAqlfunctionsGetAll}
  var url = "/_api/aqlfunction";

  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

