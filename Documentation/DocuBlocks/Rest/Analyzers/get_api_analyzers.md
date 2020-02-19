@startDocuBlock get_api_analyzers
@brief returns a listing of available Analyzer definitions

@RESTHEADER{GET /_api/analyzer, List all Analyzers, RestAnalyzerHandler:List}

@RESTDESCRIPTION
Retrieves a an array of all Analyzer definitions.
The resulting array contains objects with the following attributes:
- *name*: the Analyzer name
- *type*: the Analyzer type
- *properties*: the properties used to configure the specified type
- *features*: the set of features to set on the Analyzer generated fields

@RESTRETURNCODES

@RESTRETURNCODE{200}
The Analyzer definitions was retrieved successfully.

@EXAMPLES

Retrieve all Analyzer definitions:

@EXAMPLE_ARANGOSH_RUN{RestAnalyzersGet}
  // retrieval
  var url = "/_api/analyzer";
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
