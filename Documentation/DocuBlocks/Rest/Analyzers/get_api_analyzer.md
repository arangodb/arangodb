@startDocuBlock get_api_analyzer
@brief returns an Analyzer definition

@RESTHEADER{GET /_api/analyzer/{analyzer-name}, Return the Analyzer definition, RestAnalyzerHandler:GetDefinition}

@RESTURLPARAMETERS

@RESTURLPARAM{analyzer-name,string,required}
The name of the Analyzer to retrieve.

@RESTDESCRIPTION
Retrieves the full definition for the specified Analyzer name.
The resulting object contains the following attributes:
- *name*: the Analyzer name
- *type*: the Analyzer type
- *properties*: the properties used to configure the specified type
- *features*: the set of features to set on the Analyzer generated fields

@RESTRETURNCODES

@RESTRETURNCODE{200}
The Analyzer definition was retrieved successfully.

@RESTRETURNCODE{404}
Such an Analyzer configuration does not exist.

@EXAMPLES

Retrieve an Analyzer definition:

@EXAMPLE_ARANGOSH_RUN{RestAnalyzerGet}
  var analyzers = require("@arangodb/analyzers");
  var db = require("@arangodb").db;
  var analyzerName = "testAnalyzer";
  analyzers.save(analyzerName, "identity");

  // retrieval
  var url = "/_api/analyzer/" + encodeURIComponent(analyzerName);
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);

  logJsonResponse(response);

  analyzers.remove(analyzerName, true);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
