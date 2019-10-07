@startDocuBlock post_api_view
@brief creates an ArangoDB view

@RESTHEADER{POST /_api/view#arangosearch, Create an ArangoDB view, createView:Create}

@RESTBODYPARAM{name,string,required,string}
The name of the view.

@RESTBODYPARAM{type,string,required,string}
The type of the view. must be equal to one of the supported ArangoDB view
types.

@RESTBODYPARAM{properties,object,optional,post_api_view_props}
The view properties. If specified, then *properties* should be a JSON object
containing the attributes supported by the specific view type.

@RESTDESCRIPTION
Creates a new view with a given name and properties if it does not already
exist.

**Note**: view can't be created with the links. Please use PUT/PATCH
for links management.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestIResearchViewPostView}
    var url = "/_api/view";
    var body = {
      name: "testViewBasics",
      type: "arangosearch"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);

    db._flushCache();
    db._dropView("testViewBasics");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
