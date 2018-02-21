
@startDocuBlock post_admin_test
@brief show the available unittests on the server.

@RESTHEADER{POST /_admin/test, Runs tests on server}

@RESTALLBODYPARAM{body,object,required}
A JSON object containing an attribute *tests* which lists the files
containing the test suites.

@RESTDESCRIPTION

Executes the specified tests on the server and returns an object with the
test results. The object has an attribute "error" which states whether
any error occurred. The object also has an attribute "passed" which
indicates which tests passed and which did not.

@RESTRETURNCODE{200}
is returned when everything went well, or if a timeout occurred. In the
latter case a body of type application/json indicating the timeout
is returned.

@RESTRETURNCODE{403}
is returned if ArangoDB is not running in cluster mode.

@RESTRETURNCODE{404}
is returned if ArangoDB was not compiled for cluster operation.
@endDocuBlock

