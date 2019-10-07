@startDocuBlock get_api_analyzers
@brief returns a listing of available analyzer definitions

@RESTHEADER{GET /_api/analyzer, List all analyzers, RestAnalyzerHandler:List}

@RESTDESCRIPTION
Retrieves a an array of all analyzer definitions.
The resulting array contains objects with the following attributes:
- *name*: the analyzer name
- *type*: the analyzer type
- *properties*: the properties used to configure the specified type
- *features*: the set of features to set on the analyzer generated fields

@RESTRETURNCODES

@RESTRETURNCODE{200}
The analyzer definitions was retrieved successfully.

@EXAMPLES

Retrieve all analyzer definitions:

@EXAMPLE_ARANGOSH_RUN{RestAnalyzersGet}
  // retrieval
  var url = "/_api/analyzer";
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
