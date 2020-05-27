@startDocuBlock put_api_view_rename
@brief renames a View

@RESTHEADER{PUT /_api/view/{view-name}/rename, Rename a View, modifyView:rename}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the View to rename.

@RESTDESCRIPTION
Renames a View. Expects an object with the attribute(s)
- *name*: The new name

It returns an object with the attributes
- *id*: The identifier of the View.
- *name*: The new name of the View.
- *type*: The View type.

**Note**: This method is not available in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestViewPutRename}
    var viewName = "products1";
    var viewType = "arangosearch";

    var view = db._createView(viewName, viewType);
    var url = "/_api/view/" + view.name() + "/rename";

    var response = logCurlRequest('PUT', url, { name: "viewNewName" });

    assert(response.code === 200);
    db._flushCache();
    db._dropView("viewNewName");

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
