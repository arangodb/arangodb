
@startDocuBlock JSA_get_api_view_properties
@brief reads the properties of the specified view

@RESTHEADER{GET /_api/view/{view-name}/properties, Read properties of a view}

@RESTURLPARAMETERS

@RESTDESCRIPTION

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

Using an identifier:

    var viewName = "products";
    var viewType = "iresearch";
    var viewProperties = { locale : "c" };
    db._dropView(viewName);
    var view = db._createView(viewName, viewType, viewProperties);
    var url = "/_api/view/"+ view._id + "/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);

Using a name:

    var viewName = "products";
    var viewType = "iresearch";
    var viewProperties = { locale : "c" };
    db._dropView(viewName);
    var view = db._createView(viewName, viewType, viewProperties);
    var url = "/_api/view/products/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);

@endDocuBlock

