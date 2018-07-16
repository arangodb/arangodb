


`Controller.post(path, callback)`

Defines a new route on `path` that handles requests from the HTTP verb `post`.
This route can also be 'parameterized' like `/goose/:barn`.
In this case you can later get the value the user provided for `barn`
via the `params` function in the `request`.
The function defined in `callback` will be invoked whenever this type of
request is recieved.
`callback` get's two arguments `request` and `response`, see below for further
information about these objects.

@EXAMPLES

```js
app.post('/goose/barn', function (req, res) {
  // Take this request and deal with it!
});
```


