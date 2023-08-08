@startDocuBlock get_api_view

@RESTHEADER{GET /_api/view, List all Views, listViews}

@RESTDESCRIPTION
Returns an object containing a listing of all Views in a database, regardless
of their type. It is an array of objects with the following attributes:
- `id`
- `name`
- `type`

@RESTRETURNCODES

@RESTRETURNCODE{200}
The list of Views

@EXAMPLES

Return information about all Views:

@EXAMPLE_ARANGOSH_RUN{RestViewGetAllViews}
    var viewSearchAlias = db._createView("productsView", "search-alias");
    var viewArangoSearch = db._createView("reviewsView", "arangosearch");

    var url = "/_api/view";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);

    db._dropView("productsView");
    db._dropView("reviewsView");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
