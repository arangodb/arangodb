@startDocuBlock post_api_analyzer
@brief creates a new analyzer based on the provided definition

@RESTHEADER{POST /_api/analyzer, Create an analyzer with the suppiled definition, RestAnalyzerHandler:Create}

@RESTBODYPARAM{name,string,required,string}
The analyzer name.

@RESTBODYPARAM{type,string,required,string}
The analyzer type.

@RESTBODYPARAM{properties,string,optional,string}
The properties used to configure the specified type.
Value may be a string, an object or null.
The default value is *null*.

@RESTBODYPARAM{features,array,optional,string}
The set of features to set on the analyzer generated fields.
The default value is an empty array.

@RESTDESCRIPTION
Creates a new analyzer based on the provided configuration.

@RESTRETURNCODES

@RESTRETURNCODE{200}
An analyzer with a matching name and definition already exists.

@RESTRETURNCODE{201}
A new analyzer definition was successfully created.

@RESTRETURNCODE{400}
One or more of the required parameters is missing or one or more of the parameters
is not valid.

@RESTRETURNCODE{403}
The user does not have permission to create and analyzer with this configuration.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAnalyzerPost}
  var analyzers = require("@arangodb/analyzers");
  var db = require("@arangodb").db;
  var analyzerName = db._name() + "::testAnalyzer";

  // creation
  var url = "/_api/analyzer";
  var body = {
    name: db._name() + "::testAnalyzer",
    type: "identity"
  };
  var response = logCurlRequest('POST', url, body);
  assert(response.code === 201);

  logJsonResponse(response);

  analyzers.remove(analyzerName, true);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
