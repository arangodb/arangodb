


`Route.onlyIf(check)`

This functionality is used to secure a route by applying a checking function
on the request beforehand, for example the check authorization.
It expects `check` to be a function that takes the request object as first parameter.
This function is executed before the actual handler is invoked.
If `check` throws an error the actual handler will not be invoked.
Remember to provide an `errorResponse` on the route as well to define the behavior in this case.

*Examples*

```js
app.get("/foxx", function {
  // Do something
}).onlyIf(aFunction).errorResponse(ErrorClass, 303, "This went completely wrong. Sorry!");
```

