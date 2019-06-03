@startDocuBlock get_api_view_properties
@brief reads the properties of the specified view

@RESTHEADER{GET /_api/view/{view-name}/properties, Read properties of a view, getView}

@RESTURLPARAMETERS

@RESTDESCRIPTION
Returns an object containing the definition of the view identified by *view-name*.

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTDESCRIPTION
The result is an object describing the view with the following attributes:
- *id*: The identifier of the view
- *name*: The name of the view
- *type*: The type of the view as string
- any additional view implementation specific properties

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Using an identifier:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewPropertiesIdentifier}
    var viewName = "products";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/"+ view._id + "/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);
@END_EXAMPLE_ARANGOSH_RUN

Using a name:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewPropertiesName}
    var viewName = "products";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/products/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
