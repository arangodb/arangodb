# Making requests to external web services

As of version 2.5, ArangoDB provides a familiar API for making HTTP requests to external services. This allows you to integrate with third party APIs or to implement simple web hooks in your Foxx apps.

## Problem

I have an existing Foxx app with authentication (such as the **auth app** from the [recipe for Foxx authentication](FoxxAuth.html)) and want to use an external web service to authenticate my users instead.

## Solution

Let's say your external authentication service lives at `https://auth.example.com/login` and expects a `POST` request containing a plain old URL-encoded web form with a `username` and `password` field. It responds with a `200` status code and the user data (with a property `userId` containing a unique ID for the user and a property `profile` containing additional information about the user) if the credentials are valid and returns a `401` status code otherwise.

Next let's import the `request` module into your Foxx app. Open the file containing the controller and add `request = require('org/arangodb/request')` to the list of imports. You can then use the external service in your login route like this:

```js
controller.post('/login', function (req, res) {
  var credentials = req.params('credentials');
  var response = request.post('https://auth.example.com/login', {
    form: {
      username: credentials.get('username'),
      password: credentials.get('password')
    }
  });

  if (response.status === 200) {
    var user = JSON.parse(response.body);
    req.session.set('uid', 'auth.example.com/' + user.userId);
    req.session.set('userData', user.profile);
    res.session.save();
    res.status(201);
    res.json({
      msg: 'Welcome ' + user.profile.fullName
    });
  } else if (response.status === 401) {
    throw new NotLoggedIn();
  } else {
    console.error('Remote authentication failed (%s) with error:', response.status, response.body);
    throw new Error('An unknown error occurred!');
  }
})
.bodyParam('credentials', {description: 'Username and Password', type: Credentials})
.errorResponse(NotLoggedIn, 401, 'Login failed');
```

**Author**: [Alan Plum](https://github.com/pluma)

**Tags**: #foxx