


`response.set(key, value)`

Set a header attribute, for example:

@EXAMPLES

```js
response.set("Content-Length", 123);
response.set("Content-Type", "text/plain");
```

or alternatively:

```js
response.set({
  "Content-Length": "123",
  "Content-Type": "text/plain"
});
```

