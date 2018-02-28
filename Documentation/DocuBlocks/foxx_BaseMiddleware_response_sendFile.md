


`response.sendFile(filename, options)`

Sets the content of the specified file as the response body. The filename
must be absolute. If no content type is yet set for the response, the
response's content type will be determined automatically based
on the filename extension. If no content type is known for the extension,
the content type will default to `application/octet-stream`.

The `options` array can be used to control the behavior of sendFile.
Currently only the following option exists:
- `lastModified`: if set to true, the last modification date and time
  of the file will be returned in the `Last-Modified` HTTP header

@EXAMPLES

```js
response.sendFile('/tmp/results.json');
response.sendFile(applicationContext.fileName('image.png'), { lastModified: true });
```

