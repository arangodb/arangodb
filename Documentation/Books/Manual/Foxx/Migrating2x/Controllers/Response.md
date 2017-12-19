!CHAPTER Response objects

The response object has a lot of new methods in ArangoDB 3.0 but otherwise remains similar to the response object of previous versions:

The `res.send` method behaves very differently from how the method with the same name behaved in ArangoDB 2.x: the conversion now takes the response body definition of the route into account. There is a new method `res.write` that implements the old behaviour.

Note that consecutive calls to `res.write` will append to the response body rather than replacing it like `res.send`.

The `res.contentType` property is also no longer available. If you want to set the MIME type of the response body to an explicit value you should set the `content-type` header instead:

Old:

```js
res.contentType = 'application/json';
res.body = JSON.stringify(results);
```

New:

```js
res.set('content-type', 'application/json');
res.body = JSON.stringify(results);
```

Or simply:

```js
// sets the content type to JSON
// if it has not already been set
res.json(results);
```

The `res.cookie` method now takes the `signed` options as part of the regular options object.

Old:

```js
res.cookie('sid', 'abcdef', {
  ttl: 60 * 60,
  signed: {
    secret: 'keyboardcat',
    algorithm: 'sha256'
  }
});
```

New:

```js
res.cookie('sid', 'abcdef', {
  ttl: 60 * 60,
  secret: 'keyboardcat',
  algorithm: 'sha256'
});
```
