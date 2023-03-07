
@startDocuBlock post_api_aqlfunction
@brief create a new AQL user function

@RESTHEADER{POST /_api/aqlfunction, Create AQL user function, RestAqlUserFunctionsHandler:create}

@RESTBODYPARAM{name,string,required,string}
the fully qualified name of the user functions.

@RESTBODYPARAM{code,string,required,string}
a string representation of the function body.

@RESTBODYPARAM{isDeterministic,boolean,optional,}
an optional boolean value to indicate whether the function
results are fully deterministic (function return value solely depends on
the input value and return value is the same for repeated calls with same
input). The *isDeterministic* attribute is currently not used but may be
used later for optimizations.

@RESTDESCRIPTION

In case of success, HTTP 200 is returned.
If the function isn't valid etc. HTTP 400 including a detailed error message will be returned.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the function already existed and was replaced by the
call, the server will respond with *HTTP 200*.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{isNewlyCreated,boolean,required,}
boolean flag to indicate whether the function was newly created (*false* in this case)

@RESTRETURNCODE{201}
If the function can be registered by the server, the server will respond with
*HTTP 201*.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{isNewlyCreated,boolean,required,}
boolean flag to indicate whether the function was newly created (*true* in this case)

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing from the
request, the server will respond with *HTTP 400*.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*true* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{errorNum,integer,required,int64}
the server error number

@RESTREPLYBODY{errorMessage,string,required,string}
a descriptive error message

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAqlfunctionCreate}
  var url = "/_api/aqlfunction";
  var body = {
    name: "myfunctions::temperature::celsiustofahrenheit",
    code : "function (celsius) { return celsius * 1.8 + 32; }",
    isDeterministic: true
  };

  var response = logCurlRequest('POST', url, body);

  assert(response.code === 200 || response.code === 201 || response.code === 202);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
