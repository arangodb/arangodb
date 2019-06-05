@startDocuBlock get_api_analyzer
@brief returns an analyzer definition

@RESTHEADER{GET /_api/analyzer/{analyzer-name}, Return the analyzer definition, RestAnalyzerHandler:GetDefinition}

@RESTURLPARAMETERS

@RESTURLPARAM{analyzer-name,string,required}
The name of the analyzer to retrieve.

@RESTDESCRIPTION
Retrieves the full definition for the specified analyzer name.
The resulting object contains the following attributes:
- *name*: the analyzer name
- *type*: the analyzer type
- *properties*: the properties used to configure the specified type
- *features*: the set of features to set on the analyzer generated fields

@RESTRETURNCODES

@RESTRETURNCODE{200}
The analyzer definition was retrieved successfully.

@RESTRETURNCODE{404}
Such an analyzer configuration does not exist.

@EXAMPLES

Retrieve an analyzer definition:

@EXAMPLE_ARANGOSH_RUN{RestAnalyzerGet}
  var analyzers = require("@arangodb/analyzers");
  var db = require("@arangodb").db;
  var analyzerName = "testAnalyzer";
  analyzers.save(analyzerName, "identity", "test properties");

  // retrieval
  var url = "/_api/analyzer/" + encodeURIComponent(analyzerName);
  var response = logCurlRequest('GET', url);
  assert(response.code === 200);

  logJsonResponse(response);

  analyzers.remove(analyzerName, true);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

