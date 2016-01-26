


`Route.pathParam(id, options)`

If you defined a route "/foxx/:name", containing a parameter called `name` you can
constrain which format this parameter is allowed to have.
This format is defined using *joi* in the `options` parameter.
Using this function will at first allow you to access this parameter in your
route handler using `req.params(id)`, will reject any request having a paramter
that does not match the *joi* definition and creates a documentation for this
parameter in ArangoDBs WebInterface.

For more information on *joi* see [the official Joi documentation](https://github.com/spumko/joi).

*Parameter*

* *id*: name of the param.
* *options*: a joi schema or an object with the following properties:
 * *type*: a joi schema.
 * *description*: documentation description for the parameter.
 * *required* (optional): whether the parameter is required. Default: determined by *type*.

*Examples*

```js
app.get("/foxx/:name", function {
  // Do something
}).pathParam("name", joi.string().required().description("Name of the Foxx"));
```

You can also pass in a configuration object instead:

```js
app.get("/foxx/:name", function {
  // Do something
}).pathParam("name", {
  type: joi.string(),
  required: true,
  description: "Name of the Foxx"
});
```

