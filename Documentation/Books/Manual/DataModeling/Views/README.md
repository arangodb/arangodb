JavaScript Interface to Views
=============================

This is an introduction to ArangoDB's interface for views and how to handle
views fron the JavaScript shell _arangosh_. For other languages see the
corresponding language API.

Address of a View
-----------------

All views in ArangoDB have a unique identifier and a unique name.
ArangoDB internally uses
the view's unique identifier to look up views. This identifier, however, is
managed by ArangoDB and the user has no control over it. In order to allow users
to use their own names, each view also has a unique name which is specified by
the user. To access a view from the user perspective, the
[view name](../../Appendix/Glossary.md#view-name) should be used, i.e.:

### View
`db._view(view-name)`

A view is created by a ["db._createView"](DatabaseMethods.md) call. The returned
object may then be used via the [exposed methods](ViewMethods.md).

For example: Assume that the
[view identifier](../../Appendix/Glossary.md#view-identifier) is *7254820* and
the name is *demo*, then the view can be accessed as:

    db._view("demo")

If no view with such a name exists, then *null* is returned.

### Create
`db._createView(view-name, view-type, view-properties)`

This call will create a new view called *view-name*. This method is a database
method and is documented in detail in
[Database Methods](DatabaseMethods.md#create)

### View Types

The currently supported view implementation is: **arangosearch** as described in
[ArangoSearch View](../../Views/ArangoSearch/README.md).
