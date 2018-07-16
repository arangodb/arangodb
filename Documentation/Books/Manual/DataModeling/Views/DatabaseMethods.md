Database Methods
================

View
----

<!-- arangod/V8Server/v8-views.cpp -->

`db._view(view-name)`

Returns the view with the given name or null if no such view exists.

`db._view(view-identifier)`

Returns the view with the given identifier or null if no such view exists.
Accessing views by identifier is discouraged for end users. End users should
access views using the view name.


**Examples**

Get a view by name:

    @startDocuBlockInline viewDatabaseNameKnown
    @EXAMPLE_ARANGOSH_OUTPUT{viewDatabaseNameKnown}
      db._view("demo");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock viewDatabaseNameKnown

Get a view by id:

```
arangosh> db._view(123456);
[ArangoView 123456, "demo"]
```

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

*view-type* must be one of the supported [View Types](README.md)

*view-properties* view configuration specific to each view-type

Creates a new view named *view-name* of type *view-type* with properties
*view-properties*. If the view name already exists or if the name format is
invalid, an error is thrown. For more information on valid view names please
refer to the [naming conventions](../NamingConventions/README.md).

**Examples**

Create a view:

```
arangosh> v = db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v.properties();
arangosh> db._dropView("example");
```

All Views
---------

<!-- arangod/V8Server/v8-views.cpp -->

`db._views()`

Returns all views of the given database.


**Examples**

Query views:

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> db._views();
arangosh> db._dropView("example");
```

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

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v = db._view("example");
arangosh> db._dropView("example");
arangosh> v;
```
