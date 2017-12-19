!CHAPTER Module "request"

The request module provides the functionality for making HTTP requests.

`require('@arangodb/request')`

!SECTION Making HTTP requests

!SUBSECTION HTTP method helpers

In addition to the *request* function convenience shorthands are available for each HTTP method in the form of, i.e.:

* `request.head(url, options)`
* `request.get(url, options)`
* `request.post(url, options)`
* `request.put(url, options)`
* `request.delete(url, options)`
* `request.patch(url, options)`

These are equivalent to using the *request* function directly, i.e.:

```js
request[method](url, options)
// is equivalent to
request({method, url, ...options});
```

For example:

```js
const request = require('@arangodb/request');

request.get('http://localhost', {headers: {'x-session-id': 'keyboardcat'}});
// is equivalent to
request({
  method: 'get',
  url: 'http://localhost',
  headers: {'x-session-id': 'keyboardcat'}
});
```

!SUBSECTION The request function

The request function can be used to make HTTP requests.

`request(options)`

Performs an HTTP request and returns a *Response* object.

*Parameter*

The request function takes the following options:

* *url* or *uri*: the fully-qualified URL or a parsed URL from `url.parse`.
* *qs* (optional): object containing querystring values to be appended to the URL.
* *useQuerystring*: if `true`, use `querystring` module to handle querystrings, otherwise use `qs` module. Default: `false`.
* *method* (optional): HTTP method (case-insensitive). Default: `"GET"`.
* *headers* (optional): HTTP headers (case-insensitive). Default: `{}`.
* *body* (optional): request body. Must be a string or `Buffer`, or a JSON serializable value if *json* is `true`.
* *json*: if `true`, *body* will be serialized to a JSON string and the *Content-Type* header will be set to `"application/json"`. Additionally the response body will also be parsed as JSON (unless *encoding* is set to `null`). Default: `false`.
* *form* (optional): when set to a string or object and no *body* has been set, *body* will be set to a querystring representation of that value and the *Content-Type* header will be set to `"application/x-www-form-urlencoded"`. Also see *useQuerystring*.
* *auth* (optional): an object with the properties *username* and *password* for HTTP Basic authentication or the property *bearer* for HTTP Bearer token authentication.
* *followRedirect*: whether HTTP 3xx redirects should be followed. Default: `true`.
* *maxRedirects*: the maximum number of redirects to follow. Default: `10`.
* *encoding*: encoding to be used for the response body. If set to `null`, the response body will be returned as a `Buffer`. Default: `"utf-8"`.
* *timeout*: number of milliseconds to wait for a response before aborting the request.
* *returnBodyOnError*: whether the response body should be returned even when the server response indicates an error. Default: `true`.

The function returns a *Response* object with the following properties:

* *rawBody*: the raw response body as a `Buffer`.
* *body*: the parsed response body. If *encoding* was not set to `null`, this is a string. If additionally *json* was set to `true` and the response body is well-formed JSON, this is the parsed JSON data.
* *headers*: an object containing the response headers. Otherwise this is identical to *rawBody*.
* *statusCode* and *status*: the HTTP status code of the response, e.g. `404`.
* *message*: the HTTP status message of the response, e.g. `Not Found`.

!SUBSUBSECTION Forms

The request module supports `application/x-www-form-urlencoded` (URL encoded) form uploads:

```js
const request = require('@arangodb/request');

var res = request.post('http://service.example/upload', {form: {key: 'value'}});
// or
var res = request.post({url: 'http://service.example/upload', form: {key: 'value'}});
// or
var res = request({
  method: 'post',
  url: 'http://service.example/upload',
  form: {key: 'value'}
});
```

Form data will be encoded using the [qs](https://www.npmjs.com/package/qs) module by default.

If you want to use the [querystring](http://nodejs.org/api/querystring.html) module instead, simply use the *useQuerystring* option.

!SUBSUBSECTION JSON

If you want to submit JSON-serializable values as request bodies, just set the *json* option:

```js
const request = require('@arangodb/request');

var res = request.post('http://service.example/notify', {body: {key: 'value'}, json: true});
// or
var res = request.post({url: 'http://service.example/notify', body: {key: 'value'}, json: true});
// or
var res = request({
  method: 'post',
  url: 'http://service.example/notify',
  body: {key: 'value'},
  json: true
});
```

!SUBSUBSECTION HTTP authentication

The request module supports both *HTTP Basic* authentication. Just pass the credentials via the *auth* option:

```js
const request = require('@arangodb/request');

var res = request.get(
  'http://service.example/secret',
  {auth: {username: 'jcd', password: 'bionicman'}}
);
// or
var res = request.get({
  url: 'http://service.example/secret',
  auth: {username: 'jcd', password: 'bionicman'}
});
// or
var res = request({
  method: 'get',
  url: 'http://service.example/secret',
  auth: {username: 'jcd', password: 'bionicman'}
});
```

Alternatively you can supply the credentials via the URL:

```js
const request = require('@arangodb/request');

var username = 'jcd';
var password = 'bionicman';
var res = request.get(
  'http://' +
  encodeURIComponent(username) +
  ':' +
  encodeURIComponent(password) +
  '@service.example/secret'
);
```

You can also use *Bearer* token authentication:

```js
const request = require('@arangodb/request');

var res = request.get(
  'http://service.example/secret',
  {auth: {bearer: 'keyboardcat'}}
);
// or
var res = request.get({
  url: 'http://service.example/secret',
  auth: {bearer: 'keyboardcat'}
});
// or
var res = request({
  method: 'get',
  url: 'http://service.example/secret',
  auth: {bearer: 'keyboardcat'}
});
```
