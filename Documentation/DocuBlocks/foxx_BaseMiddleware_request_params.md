


`request.params(key)`

Get the parameters of the request. This process is two-fold:

* If you have defined an URL like */test/:id* and the user requested
  */test/1*, the call *params("id")* will return *1*.
* If you have defined an URL like */test* and the user gives a query
  component, the query parameters will also be returned.  So for example if
  the user requested */test?a=2*, the call *params("a")* will return *2*.

