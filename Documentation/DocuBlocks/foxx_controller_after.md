


`Controller.after(path, callback)`

Similar to `Controller.before(path, callback)` but `callback` will be invoked
after the request is handled in the specific route.

@EXAMPLES

```js
app.after('/high/way', function(req, res) {
  //Do some crazy response logging
});
```


