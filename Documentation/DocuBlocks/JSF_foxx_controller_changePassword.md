


FoxxController#changePassword(route, opts)`

Add a route for the logged in user to change the password.
You can provide further customizations via the
the options:

* *passwordField* can be used to adjust the expected attribute
  in the *post* request. It defaults to *password*.
* *onSuccess* is a function that you can define to do something if the change was
  successful. This includes sending a response to the user. This defaults to a
  function that returns a JSON with *notice* set to "Changed password!".
* *onError* is a function that you can define to do something if the login did not
  work. This includes sending a response to the user. This defaults to a function
  that sets the response to 401 and returns a JSON with *error* set to
  "No session was found".

Both *onSuccess* and *onError* should take request and result as arguments.

@EXAMPLES

```js
app.changePassword('/changePassword', {
  onSuccess(req, res) {
    res.json({"success": true});
  }
});
```


