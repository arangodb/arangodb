@startDocuBlock post_api_analyzer
@brief creates a new Analyzer based on the provided definition

@RESTHEADER{POST /_api/analyzer, Create an Analyzer with the supplied definition, RestAnalyzerHandler:Create}

@RESTBODYPARAM{name,string,required,string}
The Analyzer name.

@RESTBODYPARAM{type,string,required,string}
The Analyzer type.

@RESTBODYPARAM{properties,object,optional,}
The properties used to configure the specified Analyzer type.

@RESTBODYPARAM{features,array,optional,string}
The set of features to set on the Analyzer generated fields.
The default value is an empty array.

@RESTDESCRIPTION
Creates a new Analyzer based on the provided configuration.

@RESTRETURNCODES

@RESTRETURNCODE{200}
An Analyzer with a matching name and definition already exists.

@RESTRETURNCODE{201}
A new Analyzer definition was successfully created.

@RESTRETURNCODE{400}
One or more of the required parameters is missing or one or more of the parameters
is not valid.

@RESTRETURNCODE{403}
The user does not have permission to create and Analyzer with this configuration.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAnalyzerPost}
  var analyzers = require("@arangodb/analyzers");
  var db = require("@arangodb").db;
  var analyzerName = "testAnalyzer";

  // creation
  var url = "/_api/analyzer";
  var body = {
    name: "testAnalyzer",
    type: "identity"
  };
  var response = logCurlRequest('POST', url, body);
  assert(response.code === 201);

  logJsonResponse(response);

  analyzers.remove(analyzerName, true);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
