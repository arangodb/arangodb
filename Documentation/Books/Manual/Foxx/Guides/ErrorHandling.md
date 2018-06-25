Error handling
==============

Foxx automatically catches errors in your routes and generates machine-readable error responses for them, as well as logging them to the ArangoDB server log.

If the error is an `ArangoError` thrown by the ArangoDB API (such as when trying to use `collection.document` to access a document that does not exist) or explicitly thrown using the [`res.throw` method](), Foxx will convert the error to a JSON response body with an appropriate HTTP status code. Otherwise Foxx will simply generate a generic JSON error response body with a HTTP 500 status code.

Catching ArangoDB errors
------------------------

The ArangoDB JavaScript API will generally throw instances of the `ArangoError` type

Better stuff
------------

Instead:

Have application specific error objects with numeric codes to help API users, e.g.
```js
class MyFoxxError extends Error {
  constructor (code, status = 500) {
    super(`My Foxx Error #${code}`);
    this.code = code;
    this.status = status;
  }
  toJSON () {
    return {error: true, code: this.code, status: this.status};
  }
}
```

Look into `require('@arangodb').errors` when checking ArangoError codes to determine what went wrong and throw appropriate custom error.

Try to handle expected errors in each route and rethrow an application-specific error.

In root index.js have something like
```js
const UNEXPECTED_ERROR = 1234;
module.context.use((req, res, next) => {
  try {
    next();
  } catch (e) {
    if (e instanceof MyFoxxError) {
      res.status(e.status);
      res.json(e);
    } else {
      res.status(500);
      res.json(new MyFoxxError(UNEXPECTED_ERROR));
    }
  }
})
```

