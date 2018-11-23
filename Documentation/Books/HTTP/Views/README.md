HTTP Interface for Views
========================

Views
-----

This is an introduction to ArangoDB's HTTP interface for views.

### View

A view consists of documents. It is uniquely identified by its
identifier.
It also has a unique name that clients should
use to identify and access it. View can be renamed. This will
change the view name, but not the view identifier.
Views have a type that is specified by the user when the view
is created. 

The only available view type currently is: [ArangoSearch](ArangoSearch.md).

### View Identifier

A view identifier lets you refer to a view in a database.
It is a string value and is unique within the database.
ArangoDB currently uses 64bit unsigned integer values to maintain
view ids internally. When returning view ids to clients,
ArangoDB will put them into a string to ensure the view id is not
clipped by clients that do not support big integers. Clients should treat
the view ids returned by ArangoDB as opaque strings when they store
or use them locally.

### View Name

A view name identifies a view in a database. It is a string
and is unique within the database. Unlike the view identifier it is
supplied by the creator of the view . The view name must consist
of letters, digits, and the _ (underscore) and - (dash) characters only.
Please refer to Naming Conventions in ArangoDB for more information on valid
view names.

Address of a View
-----------------

All views in ArangoDB have a unique identifier and a unique
name. ArangoDB internally uses the view's unique identifier to
look up the view. This identifier however is managed by ArangoDB
and the user has no control over it. In order to allow users to use 
their own names, each view also has a unique name, which is specified
by the user. To access a view from the user perspective, the
view name should be used, i.e.:

    http://server:port/_api/view/view-name

For example: Assume that the view identifier is *7254820* and
the view name is *demo*, then the URL of that view is:

    http://localhost:8529/_api/view/demo

### View Operations

A view instance may be:
* [Created](Creating.md)
* [Retrieved](Getting.md)
* [Modified](Modifying.md)
* [Deleted](Dropping.md)
