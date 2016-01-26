

@brief delete a group of AQL user functions
`aqlfunctions.unregisterGroup(prefix)`

Unregisters a group of AQL user function, identified by a common function
group prefix.

This will return the number of functions unregistered.

@EXAMPLES

```js
  require("@arangodb/aql/functions").unregisterGroup("myfunctions::temperature");

  require("@arangodb/aql/functions").unregisterGroup("myfunctions");
```

