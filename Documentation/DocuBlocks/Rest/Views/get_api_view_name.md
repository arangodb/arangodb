
@startDocuBlock get_api_view_name
@brief returns a view

@RESTHEADER{GET /_api/view/{view-name}, Return information about a view}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the view.

@RESTDESCRIPTION
The result is an object describing the view with the following
attributes:

- *id*: The identifier of the view.

- *name*: The name of the view.

- *type*: The type of the view as string
  - arangosearch: ArangoSearch view

- *properties* : The properties of the view.

@RESTRETURNCODES

@RESTRETURNCODE{404}
If the *view-name* is unknown, then a *HTTP 404* is
returned.
@endDocuBlock

