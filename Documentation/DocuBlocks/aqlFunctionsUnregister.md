

@brief delete an existing AQL user function
`aqlfunctions.unregister(name)`

Unregisters an existing AQL user function, identified by the fully qualified
function name.

Trying to unregister a function that does not exist will result in an
exception.

@EXAMPLES

```js
  require("@arangodb/aql/functions").unregister("myfunctions::temperature::celsiustofahrenheit");
```

