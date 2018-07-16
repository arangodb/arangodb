


`Controller.allRoutes.errorResponse(errorClass, code, description)`

This is equal to invoking `Route.errorResponse` on all routes bound to this controller.

*Examples*

```js
app.allRoutes.errorResponse(FoxxyError, 303, "This went completely wrong. Sorry!");

app.get("/foxx", function {
  // Do something
});
```

