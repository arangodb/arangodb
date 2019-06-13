@startDocuBlock delete_api_analyzer
@brief removes an analyzer configuration

@RESTHEADER{DELETE /_api/analyzer/{analyzer-name}, Remove an analyzer, RestAnalyzerHandler:Delete}

@RESTURLPARAMETERS

@RESTURLPARAM{analyzer-name,string,required}
The name of the analyzer to remove.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{force,boolean,optional}
The analyzer configuration should be removed even if it is in-use.
The default value is *false*.

@RESTDESCRIPTION
Removes an analyzer configuration identified by *analyzer-name*.

If the analyzer definition was successfully dropped, an object is returned with
the following attributes:
- *error*: *false*
- *name*: The name of the removed analyzer

@RESTRETURNCODES

@RESTRETURNCODE{200}
The analyzer configuration was removed successfully.

@RESTRETURNCODE{400}
The *analyzer-name* was not supplied or another request parameter was not
valid.

@RESTRETURNCODE{403}
The user does not have permission to remove this analyzer configuration.

@RESTRETURNCODE{404}
Such an analyzer configuration does not exist.

@RESTRETURNCODE{409}
The specified analyzer configuration is still in use and *force* was omitted or
*false* specified.

@EXAMPLES

Removing without *force*:

@EXAMPLE_ARANGOSH_RUN{RestAnalyzerDelete}
  var analyzers = require("@arangodb/analyzers");
  var db = require("@arangodb").db;
  var analyzerName = "testAnalyzer";
  analyzers.save(analyzerName, "identity", "test properties");

  // removal
  var url = "/_api/analyzer/" + encodeURIComponent(analyzerName);
  var response = logCurlRequest('DELETE', url);
console.error(JSON.stringify(response));
  assert(response.code === 200);

  logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Removing with *force*:

@EXAMPLE_ARANGOSH_RUN{RestAnalyzerDeleteForce}
  var analyzers = require("@arangodb/analyzers");
  var db = require("@arangodb").db;
  var analyzerName = "testAnalyzer";
  analyzers.save(analyzerName, "identity", "test properties");

  // create analyzer reference
  var url = "/_api/collection";
  var body = { name: "testCollection" };
  var response = logCurlRequest('POST', url, body);
  assert(response.code === 200);
  var url = "/_api/view";
  var body = {
    name: "testView",
    type: "arangosearch",
    links: { testCollection: { analyzers: [ analyzerName ] } }
  };
  var response = logCurlRequest('POST', url, body);

  // removal (fail)
  var url = "/_api/analyzer/" + encodeURIComponent(analyzerName) + "?force=false";
  var response = logCurlRequest('DELETE', url);
  assert(response.code === 409);

  // removal
  var url = "/_api/analyzer/" + encodeURIComponent(analyzerName) + "?force=true";
  var response = logCurlRequest('DELETE', url);
  assert(response.code === 200);

  logJsonResponse(response);

  db._dropView("testView");
  db._drop("testCollection");
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
