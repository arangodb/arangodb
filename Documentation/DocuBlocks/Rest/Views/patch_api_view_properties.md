@startDocuBlock patch_api_view_properties
@brief partially changes properties of an ArangoDB view

@RESTHEADER{PATCH /_api/view/{view-name}/properties#arangosearch, Partially changes properties of an ArangoDB view, modifyView:partially}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTDESCRIPTION
Changes the properties of a view.

On success an object with the following attributes is returned:
- *id*: The identifier of the view
- *name*: The name of the view
- *type*: The view type
- any additional view implementation specific properties

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestViewPatchProperties}
    var viewName = "products";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/" + view.name() + "/properties";

    var response = logCurlRequest('PATCH', url, { });

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

