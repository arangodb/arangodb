


`Controller.allRoutes.onlyIfAuthenticated(code, description)`

This is equal to invoking `Route.onlyIfAuthenticated` on all routes bound to this controller.

*Examples*

```js
app.allRoutes.onlyIfAuthenticated(401, "You need to be authenticated");

app.get("/foxx", function {
  // Do something
});
```

