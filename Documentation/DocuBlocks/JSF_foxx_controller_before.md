


`Controller.before(path, callback)`

Defines an additional function on the route `path` which will be executed
before the callback defined for a specific HTTP verb is executed.
The `callback` function has the same signature as the `callback` in the
specific route.
You can also omit the `path`, in this case `callback` will be executed
before handleing any request in this Controller.

If `callback` returns the Boolean value `false`, the route handling
will not proceed. You can use this to intercept invalid or unauthorized
requests and prevent them from being passed to the matching routes.

@EXAMPLES

```js
app.before('/high/way', function(req, res) {
  //Do some crazy request logging
});
```


