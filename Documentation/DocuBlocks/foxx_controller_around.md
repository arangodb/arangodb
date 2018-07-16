


`Controller.around(path, callback)`

Similar to `Controller.before(path, callback)` `callback` will be invoked
instead of the specific handler.
`callback` takes two additional paramaters `opts` and `next` where
`opts` contains options assigned to the route and `next` is a function.
Whenever you call `next` in `callback` the specific handler is invoked,
if you do not call `next` the specific handler will not be invoked at all.
So using around you can execute code before and after a specific handler
and even call the handler only under certain circumstances.
If you omit `path` `callback` will be called on every request.

@EXAMPLES

```js
app.around('/high/way', function(req, res, opts, next) {
  //Do some crazy request logging
  next();
  //Do some more crazy request logging
});
```

