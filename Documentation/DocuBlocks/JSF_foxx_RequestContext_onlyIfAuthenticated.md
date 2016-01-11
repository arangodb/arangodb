


`FoxxController#onlyIfAuthenticated(code, reason)`

Please activate sessions for this app if you want to use this function.
Or activate authentication (deprecated).
If the user is logged in, it will do nothing. Otherwise it will respond with
the status code and the reason you provided (the route handler won't be called).
This will also add the according documentation for this route.

*Examples*

```js
app.get("/foxx", function {
  // Do something
}).onlyIfAuthenticated(401, "You need to be authenticated");
```

