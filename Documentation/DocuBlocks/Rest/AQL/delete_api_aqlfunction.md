
@startDocuBlock delete_api_aqlfunction
@brief remove an existing AQL user function

@RESTHEADER{DELETE /_api/aqlfunction/{name}, Remove existing AQL user function, RestAqlUserFunctionsHandler:Remove}

@RESTURLPARAMETERS

@RESTURLPARAM{name,string,required}
the name of the AQL user function.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{group,string,optional}
- *true*: The function name provided in *name* is treated as
  a namespace prefix, and all functions in the specified namespace will be deleted.
  The returned number of deleted functions may become 0 if none matches the string.
- *false*: The function name provided in *name* must be fully
  qualified, including any namespaces. If none matches the *name*, HTTP 404 is returned.

@RESTDESCRIPTION
Removes an existing AQL user function or function group, identified by *name*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the function can be removed by the server, the server will respond with
*HTTP 200*.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{deletedCount,integer,required,int64}
The number of deleted user functions, always `1` when `group` is set to *false*.
Any number `>= 0` when `group` is set to *true*

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

@RESTRETURNCODE{404}
If the specified user user function does not exist, the server will respond with *HTTP 404*.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*true* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{errorNum,integer,required,int64}
the server error number

@RESTREPLYBODY{errorMessage,string,required,string}
a descriptive error message

@EXAMPLES

deletes a function:

@EXAMPLE_ARANGOSH_RUN{RestAqlfunctionDelete}
  var url = "/_api/aqlfunction/square::x::y";

  var body = {
    name : "square::x::y",
    code : "function (x) { return x*x; }"
  };

  db._connection.POST("/_api/aqlfunction", body);
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 200);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

function not found:

@EXAMPLE_ARANGOSH_RUN{RestAqlfunctionDeleteFails}
  var url = "/_api/aqlfunction/myfunction::x::y";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 404);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
