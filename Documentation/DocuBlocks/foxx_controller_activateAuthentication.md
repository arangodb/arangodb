


`Controller.activateAuthentication(opts)`

To activate authentication for this controller, call this function before defining any routes.
In the `opts` object you can set the following keys:

* *type*: Currently we only support *cookie*, but this will change in the future
* *cookieLifetime*: An integer. Lifetime of cookies in seconds
* *cookieName*: A string used as the name of the cookie
* *sessionLifetime*: An integer. Lifetime of sessions in seconds

@EXAMPLES

```js
app.activateAuthentication({
  type: "cookie",
  cookieLifetime: 360000,
  cookieName: "my_cookie",
  sessionLifetime: 400,
});
```


