View Methods
============

Drop
----

<!-- arangod/V8Server/v8-views.cpp -->

`view.drop()`

Drops a *view* and all its data.

**Examples**


Drop a view:

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v = db._view("example");
arangosh> v.drop();
arangosh> v;
```

Query Name
----------

<!-- arangod/V8Server/v8-views.cpp -->

`view.name()`

Returns the name of the *view*.

**Examples**

Get view name:

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v = db._view("example");
arangosh> v.name();
arangosh> db._dropView("example");
```

Modify Name
-----------

<!-- arangod/V8Server/v8-views.cpp -->

`view.rename(new-name)`

Renames a view using the *new-name*. The *new-name* must not already be used by
a different view. *new-name* must also be a valid view name. For
more information on valid view names please refer to the
[naming conventions](../NamingConventions/README.md).

If renaming fails for any reason, an error is thrown.

**Examples**

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v = db._view("example");
arangosh> v.name();
arangosh> v.rename("example-renamed");
arangosh> v.name();
arangosh> db._dropView("example-renamed");
```

Query Type
----------

<!-- arangod/V8Server/v8-views.cpp -->

`view.type()`

Returns the type of the *view*.

**Examples**

Get view type:

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v = db._view("example");
arangosh> v.type();
arangosh> db._dropView("example");
```

Query Properties
----------------

<!-- arangod/V8Server/v8-views.cpp -->

`view.properties()`

Returns the properties of the *view*. The format of the result is specific to
each of the supported [View Types](README.md).

**Examples**

Get view properties:

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v = db._view("example");
arangosh> v.properties();
arangosh> db._dropView("example");
```

Modify Properties
-----------------

<!-- arangod/V8Server/v8-views.cpp -->

`view.properties(view-property-modification)`

Modifies the properties of the *view*. The format of the result is specific to
each of the supported [View Types](README.md).

**Examples**

Modify view properties:

```
arangosh> db._createView("example", \<view-type\>, \<view-properties\>);
arangosh> v = db._view("example");
arangosh> v.properties(\<view-property-modification\>);
arangosh> db._dropView("example");
```
