


`Controller.allRoutes.pathParam(id, options)`

This is equal to invoking `Route.pathParam` on all routes bound to this controller.

*Examples*

```js
app.allRoutes.pathParam("id", joi.string().required().description("Id of the Foxx"));

app.get("/foxx/:id", function {
  // Secured by pathParam
});
```

You can also pass in a configuration object instead:

```js
app.allRoutes.pathParam("id", {
  type: joi.string(),
  required: true,
  description: "Id of the Foxx"
});

app.get("/foxx/:id", function {
  // Secured by pathParam
});
```

