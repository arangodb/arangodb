
@startDocuBlock JSF_delete_api_aqlfunction
@brief remove an existing AQL user function

@RESTHEADER{DELETE /_api/aqlfunction/{name}, Remove existing AQL user function}

@RESTURLPARAMETERS

@RESTURLPARAM{name,string,required}
the name of the AQL user function.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{group,string,optional}
If set to *true*, then the function name provided in *name* is treated as
a namespace prefix, and all functions in the specified namespace will be deleted.
If set to *false*, the function name provided in *name* must be fully
qualified, including any namespaces.

@RESTDESCRIPTION

Removes an existing AQL user function, identified by *name*.

In case of success, the returned JSON object has the following properties:

- *error*: boolean flag to indicate that an error occurred (*false*
  in this case)

- *code*: the HTTP status code

The body of the response will contain a JSON object with additional error
details. The object has the following attributes:

- *error*: boolean flag to indicate that an error occurred (*true* in this case)

- *code*: the HTTP status code

- *errorNum*: the server error number

- *errorMessage*: a descriptive error message

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the function can be removed by the server, the server will respond with
*HTTP 200*.

@RESTRETURNCODE{400}
If the user function name is malformed, the server will respond with *HTTP 400*.

@RESTRETURNCODE{404}
If the specified user user function does not exist, the server will respond with *HTTP 404*.

@EXAMPLES

deletes a function:

@EXAMPLE_ARANGOSH_RUN{RestAqlfunctionDelete}
  var url = "/_api/aqlfunction/square::x::y";

  var body = { 
    name : "square::x::y", 
    code : "function (x) { return x*x; }" 
  };

  db._connection.POST("/_api/aqlfunction", JSON.stringify(body));
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

