


`response.send(value)`

Sets the response body to the specified *value*. If *value* is a Buffer
object, the content type will be set to `application/octet-stream` if not
yet set. If *value* is a string, the content type will be set to `text/html`
if not yet set. If *value* is an object, it will be treated as in `res.json`.

@EXAMPLES

```js
response.send({"born": "December 12, 1915"});
response.send(new Buffer("some binary data"));
response.send("<html><head><title>Hello World</title></head><body></body></html>");
```

