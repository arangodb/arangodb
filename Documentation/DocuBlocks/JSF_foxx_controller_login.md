


`FoxxController#login(path, opts)`

Add a route for the login. You can provide further customizations via the
the options:

* *usernameField* and *passwordField* can be used to adjust the expected attributes
  in the *post* request. They default to *username* and *password*.
* *onSuccess* is a function that you can define to do something if the login was
  successful. This includes sending a response to the user. This defaults to a
  function that returns a JSON with *user* set to the identifier of the user and
* *key* set to the session key.
* *onError* is a function that you can define to do something if the login did not
  work. This includes sending a response to the user. This defaults to a function
  that sets the response to 401 and returns a JSON with *error* set to
  "Username or Password was wrong".

Both *onSuccess* and *onError* should take request and result as arguments.

@EXAMPLES

```js
app.login('/login', {
  onSuccess(req, res) {
    res.json({"success": true});
  }
});
```


