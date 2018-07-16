


`Controller.allRoutes.queryParam(id, options)`

This is equal to invoking `Route.queryParam` on all routes bound to this controller.

*Examples*

```js
app.allroutes.queryParam("id",
  joi.string()
  .required()
  .description("Id of the Foxx")
  .meta({allowMultiple: false})
});

app.get("/foxx", function {
  // Do something
});
```

You can also pass in a configuration object instead:

```js
app.allroutes.queryParam("id", {
  type: joi.string().required().description("Id of the Foxx"),
  allowMultiple: false
});

app.get("/foxx", function {
  // Do something
});
```

