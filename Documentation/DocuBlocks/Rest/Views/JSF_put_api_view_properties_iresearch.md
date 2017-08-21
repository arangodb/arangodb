
@startDocuBlock JSF_put_api_view_properties_iresearch
@brief changes properties of an iresearch view

@RESTHEADER{PUT /_api/view/{view-name}/properties#iresearch, Change properties of an iresearch view}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTBODYPARAM{links,array,optional,JSF_post_api_view_links}

@RESTSTRUCT{links1,JSF_post_api_view_links,string,optional,string}
The view properties. If specified, then *properties*
should be a JSON object containing the following attributes:

@RESTBODYPARAM{locale,string,optional,string}
The default locale used for queries on analyzed string values (default: *C*).

@RESTBODYPARAM{dataPath,string,optional,string}
The filesystem path where to store persisted index data (default: *"<ArangoDB database path>/iresearch-<index id>"*).

@RESTBODYPARAM{threadMaxIdle,integer,optional,uint64}
Maximum idle number of threads for single-run tasks (default: 5).
For the case where there are a lot of short-lived asynchronous tasks, a lower value will cause a lot of thread creation/deletion calls.
For the case where there are no short-lived asynchronous tasks, a higher value will only waste memory.

@RESTBODYPARAM{threadMaxTotal,integer,optional,uint64}
Maximum total number of threads (>0) for single-run tasks (default: 5).
For the case where there are a lot of parallelizable tasks and an abundance of resources, a lower value would limit performance.
For the case where there are limited resources CPU/memory, a higher value will negatively impact performance.

@RESTDESCRIPTION
Changes the properties of a view.

On success an object with the following attributes is returned:

- *id*: The identifier of the view.

- *name*: The name of the view.

- *type*: The view type. Valid types are:
  - iresearch : IResearch view

- *properties*: The updated properties of the view.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *view-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

    var viewName = "products";
    var viewType = "iresearch";
    db._dropView(viewName);
    var view = db._createView(viewName, viewType, {});
    var url = "/_api/view/"+ view.name() + "/properties";

    var response = logCurlRequest('PUT', url, { "threadMaxIdle" : 10 });

    assert(response.code === 200);

    logJsonResponse(response);
    db._dropView(viewName);
@endDocuBlock

