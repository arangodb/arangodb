
@startDocuBlock JSF_delete_api_view
@brief drops a view

@RESTHEADER{DELETE /_api/view/{view-name}, Drops a view}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view to drop.

@RESTQUERYPARAMETERS

@RESTDESCRIPTION
Drops the view identified by *view-name*.

If the view was successfully dropped, an object is returned with
the following attributes:

- *error*: *false*

- *id*: The identifier of the dropped view.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Using an identifier:

    var viewName = "testView";
    var viewType = "iresearch";
    var viewProperties = {};
    var view = db._createView(viewName, viewType, viewProperties);
    var url = "/_api/view/"+ view._id;

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 200);

    logJsonResponse(response);

Using a name:

    var viewName = "testView";
    var viewType = "iresearch";
    var viewProperties = {};
    db._dropView(viewName);
    var view = db._createView(viewName, viewType, viewProperties);
    var url = "/_api/view/testView";

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 200);

    logJsonResponse(response);

@endDocuBlock

