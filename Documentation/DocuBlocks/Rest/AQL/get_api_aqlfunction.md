
@startDocuBlock get_api_aqlfunction
@brief gets all reqistered AQL user functions

@RESTHEADER{GET /_api/aqlfunction, Return registered AQL user functions, RestAqlUserFunctionsHandler:List}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{namespace,string,optional}
Returns all registered AQL user functions from namespace *namespace* under *result*.

@RESTDESCRIPTION
Returns all registered AQL user functions.

The call will return a JSON array with status codes and all user functions found under *result*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
on success *HTTP 200* is returned.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{result,array,required,aql_userfunction_struct}
All functions, or the ones matching the *namespace* parameter

@RESTSTRUCT{name,aql_userfunction_struct,string,required,}
The fully qualified name of the user function

@RESTSTRUCT{code,aql_userfunction_struct,string,required,}
A string representation of the function body

@RESTSTRUCT{isDeterministic,aql_userfunction_struct,boolean,required,}
an optional boolean value to indicate whether the function
results are fully deterministic (function return value solely depends on
the input value and return value is the same for repeated calls with same
input). The *isDeterministic* attribute is currently not used but may be
used later for optimizations.

@RESTRETURNCODE{400}
If the user function name is malformed, the server will respond with *HTTP 400*.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*true* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{errorNum,integer,required,int64}
the server error number

@RESTREPLYBODY{errorMessage,string,required,string}
a descriptive error message

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAqlfunctionsGetAll}
  var url = "/_api/aqlfunction/test";

  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
