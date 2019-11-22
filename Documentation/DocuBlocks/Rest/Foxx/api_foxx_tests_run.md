@startDocuBlock api_foxx_tests_run
@brief run service tests

@RESTHEADER{POST /_api/foxx/tests, Run service tests}

@RESTDESCRIPTION
Runs the tests for the service at the given mount path and returns the results.

Supported test reporters are:

- *default*: a simple list of test cases
- *suite*: an object of test cases nested in suites
- *stream*: a raw stream of test results
- *xunit*: an XUnit/JUnit compatible structure
- *tap*: a raw TAP compatible stream

The *Accept* request header can be used to further control the response format:

When using the *stream* reporter `application/x-ldjson` will result
in the response body being formatted as a newline-delimited JSON stream.

When using the *tap* reporter `text/plain` or `text/*` will result
in the response body being formatted as a plain text TAP report.

When using the *xunit* reporter `application/xml` or `text/xml` will result
in the response body being formatted as XML instead of JSONML.

Otherwise the response body will be formatted as non-prettyprinted JSON.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{mount,string,required}
Mount path of the installed service.

@RESTQUERYPARAM{reporter,string,optional}
Test reporter to use.

@RESTQUERYPARAM{idiomatic,boolean,optional}
Use the matching format for the reporter, regardless of the *Accept* header.

@RESTQUERYPARAM{filter,string,optional}
Only run tests where the full name (including full test suites and test case)
matches this string.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the request was successful.

@endDocuBlock
