


`Controller.allRoutes.onlyIf(code, reason)`

This is equal to invoking `Route.onlyIf` on all routes bound to this controller.

*Examples*

```js
app.allRoutes.onlyIf(myPersonalCheck);

app.get("/foxx", function {
  // Do something
});
```

