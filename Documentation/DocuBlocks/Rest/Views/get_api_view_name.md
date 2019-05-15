@startDocuBlock get_api_view_name
@brief returns a view

@RESTHEADER{GET /_api/view/{view-name}, Return information about a view, getViews:Properties}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTDESCRIPTION
The result is an object describing the view with the following attributes:
- *id*: The identifier of the view
- *name*: The name of the view
- *type*: The type of the view as string

@RESTRETURNCODES

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Using an identifier:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewIdentifier}
    var viewName = "testView";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/"+ view._id;

    var response = logCurlRequest('GET', url);
    assert(response.code === 200);

    logJsonResponse(response);
    
    db._dropView("testView");
@END_EXAMPLE_ARANGOSH_RUN

Using a name:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewName}
    var viewName = "testView";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/testView";

    var response = logCurlRequest('GET', url);
    assert(response.code === 200);

    logJsonResponse(response);

    db._dropView("testView");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
