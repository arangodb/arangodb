!CHAPTER Response objects

The response object specifies the following properties:

* **body**: `Buffer | string`

  Response body as a string or buffer. Can be set directly or using some of the response methods.

* **context**: `Context`

  The [service context](../Context.md) in which the router was mounted (rather than the context in which the route was defined).

* **headers**: `object`

  The raw headers object.

* **statusCode**: `number`

  Status code of the response. Defaults to `200` (body set and not an empty string or buffer) or `204` (otherwise) if not changed from `undefined`.

!SECTION attachment

`res.attachment([filename]): this`

Sets the `content-disposition` header to indicate the response is a downloadable file with the given name.

**Arguments**

* **filename**: `string` (optional)

  Name of the downloadable file in the response body.

  If present, the extension of the filename will be used to set the response `content-type` if it has not yet been set.

Returns the response object.

!SECTION cookie

`res.cookie(name, value, [options]): this`

Sets a cookie with the given name.

**Arguments**

* **name**: `string`

  Name of the cookie.

* **value**: `string`

  Value of the cookie.

* **options**: `object` (optional)

  An object with any of the following properties:

  * **secret**: `string` (optional)

    Secret that will be used to sign the cookie.

    If a secret is specified, the cookie's signature will be stored in a second cookie with the same options, the same name and the suffix `.sig`. Otherwise no signature will be added.

  * **algorithm**: `string` (Default: `"sha256"`)

    Algorithm that will be used to sign the cookie.

  * **ttl**: `number` (optional)

    Time to live of the cookie.

  * **path**: `number` (optional)

    Path of the cookie.

  * **domain**: `number` (optional)

    Domain of the cookie.

  * **secure**: `number` (Default: `false`)

    Whether the cookie should be marked as secure (i.e. HTTPS/SSL-only).

  * **httpOnly**: `boolean` (Default: `false`)

    Whether the cookie should be marked as HTTP-only.

If a string is passed instead of an options object it will be interpreted as the *secret* option.

If a number is passed instead of an options object it will be interpreted as the *ttl* option.

Returns the response object.

!SECTION download

`res.download(path, [filename]): this`

The equivalent of calling `res.attachment(filename).sendFile(path)`.

**Arguments**

* **path**: `string`

  Path to the file on the local filesystem to be sent as the response body.

* **filename**: `string` (optional)

  Filename to indicate in the `content-disposition` header.

  If omitted the *path* will be used instead.

Returns the response object.

!SECTION getHeader

`res.getHeader(name): string`

Gets the value of the header with the given name.

**Arguments**

* **name**: `string`

  Name of the header to get.

Returns the value of the header or `undefined`.

!SECTION json

`res.json(data): this`

Sets the response body to the JSON string value of the given data.

**Arguments**

* **data**: `any`

  The data to be used as the response body.

Returns the response object.

!SECTION redirect

`res.redirect([status], path): this`

Redirects the response by setting the response `location` header and status code.

**Arguments**

* **status**: `number | string` (optional)

  Response status code to set.

  If the status code is the string value `"permanent"` it will be treated as the value `301`.

  If the status code is a string it will be converted to a numeric status code using the [statuses module](https://github.com/jshttp/statuses) first.

  If the status code is omitted but the response status has not already been set, the response status will be set to `302`.

* **path**: `string`

  URL to set the `location` header to.

Returns the response object.

!SECTION removeHeader

`res.removeHeader(name): this`

Removes the header with the given name from the response.

**Arguments**

* **name**: `string`

  Name of the header to remove.

Returns the response object.

!SECTION send

`res.send(data, [type]): this`

Sets the response body to the given data with respect to the response definition for the response's current status code.

**Arguments**

* **data**: `any`

  The data to be used as the response body. Will be converted according the [response definition](Endpoints.md#response) for the response's current status code (or `200`) in the following way:

  If the data is an ArangoDB result set, it will be converted to an array first.

  If the response definition specifies a model with a `forClient` method, that method will be applied to the data first. If the data is an array and the response definition has the `multiple` flag set, the method will be applied to each entry individually instead.

  Finally the data will be processed by the response type handler to conver the response body to a string or buffer.

* **type**: `string` (Default: `"auto"`)

  Content-type of the response body.

  If set to `"auto"` the first MIME type specified in the [response definition](Endpoints.md#response) for the response's current status code (or `200`) will be used instead.

  If set to `"auto"` and no response definition exists, the MIME type will be determined the following way:

  If the data is a buffer the MIME type will be set to binary (`application/octet-stream`).

  If the data is an object the MIME type will be set to JSON and the data will be converted to a JSON string.

  Otherwise the MIME type will be set to HTML and the data will be converted to a string.

Returns the response object.

!SECTION sendFile

`res.sendFile(path, [options]): this`

Sends a file from the local filesystem as the response body.

**Arguments**

* **path**: `string`

  Path to the file on the local filesystem to be sent as the response body.

  If no `content-type` header has been set yet, the extension of the filename will be used to set the value of that header.

* **options**: `object` (optional)

  An object with any of the following properties:

  * **lastModified**: `boolean` (optional)

    If set to `true` or if no `last-modified` header has been set yet and the value is not set to `false`
    the `last-modified` header will be set to the modification date of the file in milliseconds.

Returns the response object.

**Examples**

```js
// Send the file "favicon.ico" from this service's folder
res.sendFile(module.context.fileName('favicon.ico'));
```

!SECTION sendStatus

`res.sendStatus(status): this`

Sends a plaintext response for the given status code.
The response status will be set to the given status code, the response body will be set to the status message corresponding to that status code.

**Arguments**

* **status**: `number | string`

  Response status code to set.

  If the status code is a string it will be converted to a numeric status code using the [statuses module](https://github.com/jshttp/statuses) first.

Returns the response object.

!SECTION setHeader / set

`res.setHeader(name, value): this`

`res.set(name, value): this`

`res.set(headers): this`

Sets the value of the header with the given name.

**Arguments**

* **name**: `string`

  Name of the header to set.

* **value**: `string`

  Value to set the header to.

* **headers**: `object`

  Header object mapping header names to values.

Returns the response object.

!SECTION status

`res.status(status): this`

Sets the response status to the given status code.

**Arguments**

* **status**: `number | string`

  Response status code to set.

  If the status code is a string it will be converted to a numeric status code using the [statuses module](https://github.com/jshttp/statuses) first.

Returns the response object.

!SECTION throw

`res.throw(status, [reason], [options]): void`

Throws an HTTP exception for the given status, which will be handled by Foxx to serve the appropriate JSON error response.

**Arguments**

* **status**: `number | string`

  Response status code to set.

  If the status code is a string it will be converted to a numeric status code using the [statuses module](https://github.com/jshttp/statuses) first.

  If the status code is in the 500-range (500-599), its stacktrace will always be logged as if it were an unhandled exception.

  If development mode is enabled, the error's stacktrace will be logged as a warning if the status code is in the 400-range (400-499) or as a regular message otherwise.

* **reason**: `string` (optional)

  Message for the exception.

  If omitted, the status message corresponding to the status code will be used instead.

* **options**: `object` (optional)

  An object with any of the following properties:

  * **cause**: `Error` (optional)

    Cause of the exception that will be logged as part of the error's stacktrace (recursively, if the exception also has a `cause` property and so on).

  * **extra**: `object` (optional)

    Additional properties that will be added to the error response body generated by Foxx.

    If development mode is enabled, an `exception` property will be added to this value containing the error message and a `stacktrace` property will be added containing an array with each line of the error's stacktrace.

If an error is passed instead of an options object it will be interpreted as the *cause* option. If no reason was provided the error's `message` will be used as the reason instead.

Returns nothing.

!SECTION vary

`res.vary(names): this`

`res.vary(...names): this`

This method wraps the `vary` header manipulation method of the [vary module](https://github.com/jshttp/vary) for the current response.

The given names will be added to the response's `vary` header if not already present.

Returns the response object.

**Examples**

```js
res.vary('user-agent');
res.vary('cookie');
res.vary('cookie'); // duplicates will be ignored

// -- or --

res.vary('user-agent', 'cookie');

// -- or --

res.vary(['user-agent', 'cookie']);
```

!SECTION write

`res.write(data): this`

Appends the given data to the response body.

**Arguments**

* **data**: `string | Buffer`

  Data to append.

  If the data is a buffer the response body will be converted to a buffer first.

  If the response body is a buffer the data will be converted to a buffer first.

  If the data is an object it will be converted to a JSON string first.

  If the data is any other non-string value it will be converted to a string first.

Returns the response object.
