


`FoxxController#register(path, opts)`

This works pretty similar to the logout function and adds a path to your
app for the register functionality. You can customize it with a custom *onSuccess*
and *onError* function:

* *onSuccess* is a function that you can define to do something if the registration was
  successful. This includes sending a response to the user. This defaults to a
  function that returns a JSON with *user* set to the created user document.
* *onError* is a function that you can define to do something if the registration did not
  work. This includes sending a response to the user. This defaults to a function
  that sets the response to 401 and returns a JSON with *error* set to
  "Registration failed".

Both *onSuccess* and *onError* should take request and result as arguments.

You can also set the fields containing the username and password via *usernameField*
(defaults to *username*) and *passwordField* (defaults to *password*).
If you want to accept additional attributes for the user document, use the option
*acceptedAttributes* and set it to an array containing strings with the names of
the additional attributes you want to accept. All other attributes in the request
will be ignored.

If you want default attributes for the accepted attributes or set additional fields
(for example *admin*) use the option *defaultAttributes* which should be a hash
mapping attribute names to default values.

@EXAMPLES

```js
app.register('/logout', {
  acceptedAttributes: ['name'],
  defaultAttributes: {
    admin: false
  }
});
```


