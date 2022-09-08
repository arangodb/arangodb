@startDocuBlock post_api_view_searchalias
@brief Creates a `search-alias` View

@RESTHEADER{POST /_api/view#searchalias, Create a Search Alias View, createViewSearchAlias}

@RESTBODYPARAM{name,string,required,string}
The name of the View.

@RESTBODYPARAM{type,string,required,string}
The type of the View. Must be equal to `"search-alias"`.
This option is immutable.

@RESTBODYPARAM{indexes,array,optional,post_api_view_searchalias_indexes}
A list of inverted indexes to add to the View.

@RESTSTRUCT{collection,post_api_view_searchalias_indexes,string,required,}
The name of a collection.

@RESTSTRUCT{index,post_api_view_searchalias_indexes,string,required,}
The name of an inverted index of the `collection`.

@RESTDESCRIPTION
Creates a new View with a given name and properties if it does not
already exist.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *name* or *type* attribute are missing or invalid, then an *HTTP 400*
error is returned.

@RESTRETURNCODE{409}
If a View called *name* already exists, then an *HTTP 409* error is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestViewPostViewSearchAlias}
    var url = "/_api/view";
    var body = {
      name: "testViewBasics",
      type: "search-alias"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);

    db._flushCache();
    db._dropView("testViewBasics");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
