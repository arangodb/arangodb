


`Route.queryParam(id, options)`

Describe a query parameter:

If you defined a route "/foxx", you can allow a query paramter with the
name `id` on it and constrain the format of this parameter by giving it a *joi* type in the `options` parameter.
Using this function will at first allow you to access this parameter in your
route handler using `req.params(id)`, will reject any request having a paramter
that does not match the *joi* definition and creates a documentation for this
parameter in ArangoDBs WebInterface.

For more information on *joi* see [the official Joi documentation](https://github.com/spumko/joi).

You can also provide a description of this parameter and
whether you can provide the parameter multiple times.

*Parameter*

* *id*: name of the parameter
* *options*: a joi schema or an object with the following properties:
 * *type*: a joi schema
 * *description*: documentation description for this param.
 * *required* (optional): whether the param is required. Default: determined by *type*.
 * *allowMultiple* (optional): whether the param can be specified more than once. Default: `false`.

*Examples*

```js
app.get("/foxx", function {
  // Do something
}).queryParam("id",
  joi.string()
  .required()
  .description("Id of the Foxx")
  .meta({allowMultiple: false})
});
```

You can also pass in a configuration object instead:

```js
app.get("/foxx", function {
  // Do something
}).queryParam("id", {
  type: joi.string().required().description("Id of the Foxx"),
  allowMultiple: false
});
```

