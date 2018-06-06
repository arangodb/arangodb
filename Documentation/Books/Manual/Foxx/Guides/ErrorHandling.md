# Error handling

Foxx will automatically generate machine-readable error responses when an error isn't caught but this is likely not what you want to expose to API users. Instead:

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

