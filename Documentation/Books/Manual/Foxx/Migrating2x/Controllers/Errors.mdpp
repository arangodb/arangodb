!CHAPTER Error handling

The `errorResponse` method provided by controller request contexts has no equivalent in router endpoints. If you want to handle specific error types with specific status codes you need to catch them explicitly, either in the route or in a middleware:

Old:

```js
ctrl.get('/puppies', function (req, res) {
  // Exception is thrown here
})
.errorResponse(TooManyPuppiesError, 400, 'Something went wrong!');
```

New:

```js
ctrl.get('/puppies', function (req, res) {
  try {
    // Exception is thrown here
  } catch (e) {
    if (!(e instanceof TooManyPuppiesError)) {
      throw e;
    }
    res.throw(400, 'Something went wrong!');
  }
})
// The "error" method merely documents the meaning
// of the status code and has no other effect.
.error(400, 'Thrown if there are too many puppies.');
```

Note that errors created with `http-errors` are still handled by Foxx intelligently. In fact `res.throw` is just a helper method for creating and throwing these errors.
