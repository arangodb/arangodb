
@startDocuBlock JSF_post_api_aqlfunction
@brief create a new AQL user function

@RESTHEADER{POST /_api/aqlfunction, Create AQL user function}

@RESTBODYPARAM{name,string,required,string}
the fully qualified name of the user functions.

@RESTBODYPARAM{code,string,required,string}
a string representation of the function body.

@RESTBODYPARAM{isDeterministic,boolean,optional,}
an optional boolean value to indicate that the function
results are fully deterministic (function return value solely depends on
the input value and return value is the same for repeated calls with same
input). The *isDeterministic* attribute is currently not used but may be
used later for optimisations.

@RESTDESCRIPTION

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
If the function already existed and was replaced by the
call, the server will respond with *HTTP 200*.

@RESTRETURNCODE{201}
If the function can be registered by the server, the server will respond with
*HTTP 201*.

@RESTRETURNCODE{400}
If the JSON representation is malformed or mandatory data is missing from the
request, the server will respond with *HTTP 400*.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAqlfunctionCreate}
  var url = "/_api/aqlfunction";
  var body = {
    name: "myfunctions::temperature::celsiustofahrenheit",
    code : "function (celsius) { return celsius * 1.8 + 32; }"
  };

  var response = logCurlRequest('POST', url, body);

  assert(response.code === 200 || response.code === 201 || response.code === 202);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

