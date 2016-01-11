

@brief list all AQL user functions
`aqlfunctions.toArray()`

Returns all previously registered AQL user functions, with their fully
qualified names and function code.

The result may optionally be restricted to a specified group of functions
by specifying a group prefix:

`aqlfunctions.toArray(prefix)`

@EXAMPLES

To list all available user functions:

```js
  require("@arangodb/aql/functions").toArray();
```

To list all available user functions in the *myfunctions* namespace:

```js
  require("@arangodb/aql/functions").toArray("myfunctions");
```

To list all available user functions in the *myfunctions::temperature* namespace:

```js
  require("@arangodb/aql/functions").toArray("myfunctions::temperature");
```

