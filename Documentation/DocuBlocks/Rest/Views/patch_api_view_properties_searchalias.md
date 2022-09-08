@startDocuBlock patch_api_view_properties_searchalias
@brief Partially changes properties of a `search-alias` View

@RESTHEADER{PATCH /_api/view/{view-name}/properties#searchalias, Partially changes properties of a Search Alias View, modifyViewPartialSearchAlias}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the View.

@RESTBODYPARAM{indexes,array,optional,patch_api_view_searchalias_indexes}
A list of inverted indexes to add to or remove from the View.

@RESTSTRUCT{collection,patch_api_view_searchalias_indexes,string,required,}
The name of a collection.

@RESTSTRUCT{index,patch_api_view_searchalias_indexes,string,required,}
The name of an inverted index of the `collection`.

@RESTSTRUCT{operation,patch_api_view_searchalias_indexes,string,optional,}
Whether to add or remove the index to the stored `indexes` property of the View.
Possible values: `"add"`, `"del"`. The default is `"add"`.

@RESTDESCRIPTION
Updates the list of indexes of a `search-alias` View.

@RESTRETURNCODES

@RESTRETURNCODE{200}
On success, an object with the following attributes is returned:

@RESTREPLYBODY{id,string,required,}
The identifier of the View.

@RESTREPLYBODY{name,string,required,}
The name of the View.

@RESTREPLYBODY{type,string,required,}
The View type (`"search-alias"`).

@RESTREPLYBODY{indexes,array,required,patch_api_view_searchalias_indexes_reply}
The list of inverted indexes that are part of the View.

@RESTSTRUCT{collection,patch_api_view_searchalias_indexes_reply,string,required,}
The name of a collection.

@RESTSTRUCT{index,patch_api_view_searchalias_indexes_reply,string,required,}
The name of an inverted index of the `collection`.

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestViewPatchPropertiesSearchAlias}
    var viewName = "products";
    var viewType = "search-alias";
    var indexName1 = "inv_title";
    var indexName2 = "inv_descr";

    var coll = db._create("books");
    coll.ensureIndex({ type: "inverted", name: indexName1, fields: ["title"] });
    coll.ensureIndex({ type: "inverted", name: indexName2, fields: ["description"] });

    var view = db._createView(viewName, viewType, {
      indexes: [ { collection: coll.name(), index: indexName1 } ] });

    var url = "/_api/view/"+ view.name() + "/properties";
    var response = logCurlRequest('PATCH', url, {
      "indexes": [ { collection: coll.name(), index: indexName2 } ] });

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);
    db._drop(coll.name());
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
