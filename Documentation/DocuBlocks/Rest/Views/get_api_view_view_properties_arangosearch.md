@startDocuBlock get_api_view_view_properties_arangosearch

@RESTHEADER{GET /_api/view/{view-name}/properties, Get the properties of a View, getViewProperties}

@RESTURLPARAMETERS

@RESTDESCRIPTION
Returns an object containing the definition of the View identified by `view-name`.

@RESTURLPARAM{view-name,string,required}
The name of the View.

@RESTDESCRIPTION
The result is an object with a full description of a specific View, including
View type dependent properties.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the `view-name` is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the `view-name` is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Using an identifier:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewPropertiesIdentifierArangoSearch}
    var coll = db._create("books");
    var view = db._createView("productsView", "arangosearch", { links: { books: { fields: { title: { analyzers: ["text_en"] } } } } });

    var url = "/_api/view/"+ view._id + "/properties";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);

    db._dropView("productsView");
    db._drop("books");
@END_EXAMPLE_ARANGOSH_RUN

Using a name:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewPropertiesNameArangoSearch}
    var coll = db._create("books");
    var view = db._createView("productsView", "arangosearch", { links: { books: { fields: { title: { analyzers: ["text_en"] } } } } });

    var url = "/_api/view/productsView/properties";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);

    db._dropView("productsView");
    db._drop("books");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
