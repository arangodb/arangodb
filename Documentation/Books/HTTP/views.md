---
layout: default
description: This is an introduction to ArangoDB's HTTP interface for views
---
HTTP Interface for Views
========================

Views
-----

This is an introduction to ArangoDB's HTTP interface for Views.

### View

A View consists of documents. It is uniquely identified by its
identifier.
It also has a unique name that clients should
use to identify and access it. View can be renamed. This will
change the View name, but not the View identifier.
Views have a type that is specified by the user when the View
is created. 

The only available View type currently is [ArangoSearch](../arangosearch-views.html).

### View Identifier

A View identifier lets you refer to a View in a database.
It is a string value and is unique within the database.
ArangoDB currently uses 64bit unsigned integer values to maintain
View ids internally. When returning View ids to clients,
ArangoDB will put them into a string to ensure the View id is not
clipped by clients that do not support big integers. Clients should treat
the View ids returned by ArangoDB as opaque strings when they store
or use them locally.

### View Name

A View name identifies a View in a database. It is a string
and is unique within the database. Unlike the View identifier it is
supplied by the creator of the View . The View name must consist
of letters, digits, and the _ (underscore) and - (dash) characters only.
Please refer to Naming Conventions in ArangoDB for more information on valid
View names.

Address of a View
-----------------

All Views in ArangoDB have a unique identifier and a unique
name. ArangoDB internally uses the View's unique identifier to
look up the View. This identifier however is managed by ArangoDB
and the user has no control over it. In order to allow users to use 
their own names, each View also has a unique name, which is specified
by the user. To access a View from the user perspective, the
View name should be used, i.e.:

    http://server:port/_api/view/<view-name>

For example: Assume that the View identifier is *7254820* and
the View name is *demo*, then the URL of that View is:

    http://localhost:8529/_api/view/demo
