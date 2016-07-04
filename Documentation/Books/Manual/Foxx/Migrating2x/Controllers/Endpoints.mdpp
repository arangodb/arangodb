!CHAPTER The request context

When defining a route on a controller the controller would return an object called *request context*. Routers return a similar object called *endpoint*. Routers also return endpoints when mounting child routers or middleware, as does the `use` method of the service context.

The main differences between the new endpoints and the objects returned by controllers in previous versions of ArangoDB are:

* `bodyParam` is now simply called `body`; it is no longer neccessary or possible to give the body a name and the request body will not show up in the request parameters. It's also possible to specify a MIME type

* `body`, `queryParam` and `pathParam` now take position arguments instead of an object. For specifics see the [endpoint documentation](../../Router/Endpoints.md).

* `notes` is now called `description` and takes a single string argument.

* `onlyIf` and `onlyIfAuthenticated` are no longer available; they can be emulated with middleware if necessary:

Old:

```js
ctrl.get(/* ... */)
.onlyIf(function (req) {
  if (!req.user) {
    throw new Error('Not authenticated!');
  }
});
```

New:

```js
router.use(function (req, res, next) {
  if (!req.arangoUser) {
    res.throw(403, 'Not authenticated!');
  }
  next();
});

router.get(/* ... */);
```
