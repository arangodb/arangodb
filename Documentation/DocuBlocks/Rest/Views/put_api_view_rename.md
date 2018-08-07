@startDocuBlock put_api_view_rename
@brief renames a view

@RESTHEADER{PUT /_api/view/{view-name}/rename, Rename a view}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view to rename.

@RESTDESCRIPTION
Renames a view. Expects an object with the attribute(s)
- *name*: The new name

It returns an object with the attributes
- *id*: The identifier of the view.
- *name*: The new name of the view.
- *type*: The view type.

**Note**: this method is not available in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestViewPutViewRename}
    var viewName = "products1";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/" + view.name() + "/rename";

    var response = logCurlRequest('PUT', url, { name: "viewNewName" });

    assert(response.code === 200);
    db._flushCache();
    db._dropView(viewNewName);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
