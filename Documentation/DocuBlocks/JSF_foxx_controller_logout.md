


`FoxxController#logout(path, opts)`

This works pretty similar to the logout function and adds a path to your
app for the logout functionality. You can customize it with a custom *onSuccess*
and *onError* function:

* *onSuccess* is a function that you can define to do something if the logout was
  successful. This includes sending a response to the user. This defaults to a
  function that returns a JSON with *message* set to "logged out".
* *onError* is a function that you can define to do something if the logout did not
  work. This includes sending a response to the user. This defaults to a function
  that sets the response to 401 and returns a JSON with *error* set to
  "No session was found".

Both *onSuccess* and *onError* should take request and result as arguments.


@EXAMPLES

```js
app.logout('/logout', {
  onSuccess(req, res) {
    res.json({"message": "Bye, Bye"});
  }
});
```


