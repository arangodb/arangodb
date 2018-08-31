Database Methods
================

View
----

<!-- arangod/V8Server/v8-views.cpp -->

`db._view(view-name)`

Returns the view with the given name or null if no such view exists.

    @startDocuBlockInline viewDatabaseGet
    @EXAMPLE_ARANGOSH_OUTPUT{viewDatabaseGet}
    ~ db._createView("example", "arangosearch", {});
      | view = db._view("example");
      // or, alternatively
      view = db["example"]
    ~ db._dropView("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewDatabaseGet

`db._view(view-identifier)`

Returns the view with the given identifier or null if no such view exists.
Accessing views by identifier is discouraged for end users. End users should
access views using the view name.


**Examples**

Get a view by name:

    @startDocuBlockInline viewDatabaseNameKnown
    @EXAMPLE_ARANGOSH_OUTPUT{viewDatabaseNameKnown}
      db._view("demoView");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewDatabaseNameKnown

Unknown view:

    @startDocuBlockInline viewDatabaseNameUnknown
    @EXAMPLE_ARANGOSH_OUTPUT{viewDatabaseNameUnknown}
      db._view("unknown");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewDatabaseNameUnknown


Create
------

<!-- arangod/V8Server/v8-views.cpp -->

`db._createView(view-name, view-type, view-properties)`

Creates a new view named *view-name* of type *view-type* with properties
*view-properties*.

*view-name* is a string and the name of the view. No view or collection with the
same name may already exist in the current database. For more information on
valid view names please refer to the [naming conventions
](../NamingConventions/README.md).

*view-type* must be the string `"arangosearch"`, as it is currently the only
supported view type.

*view-properties* is an optional object containing view configuration specific
to each view-type. Currently, only ArangoSearch Views are supported. See
[ArangoSearch View definition
](../../Views/ArangoSearch/DetailedOverview.md#view-definitionmodification) for
details.

**Examples**

    @startDocuBlockInline viewDatabaseCreate
    @EXAMPLE_ARANGOSH_OUTPUT{viewDatabaseCreate}
      v = db._createView("example", "arangosearch");
      v.properties()
      db._dropView("example")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewDatabaseCreate


All Views
---------

<!-- arangod/V8Server/v8-views.cpp -->

`db._views()`

Returns all views of the given database.


**Examples**

List all views:

    @startDocuBlockInline viewDatabaseList
    @EXAMPLE_ARANGOSH_OUTPUT{viewDatabaseList}
    ~ db._createView("exampleView", "arangosearch");
      db._views();
    ~ db._dropView("exampleView");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewDatabaseList

Drop
----

<!-- arangod/V8Server/v8-views.cpp -->

`db._dropView(view-name)`

Drops a view named *view-name* and all its data. No error is thrown if there is
no such view.

`db._dropView(view-identifier)`

Drops a view identified by *view-identifier* with all its data. No error is
thrown if there is no such view.

**Examples**

Drop a view:

    @startDocuBlockInline viewDatabaseDrop
    @EXAMPLE_ARANGOSH_OUTPUT{viewDatabaseDrop}
      db._createView("exampleView", "arangosearch");
      db._dropView("exampleView");
      db._view("exampleView");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewDatabaseDrop
