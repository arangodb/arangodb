# simple-get [![travis][travis-image]][travis-url] [![npm][npm-image]][npm-url] [![downloads][downloads-image]][downloads-url]

[travis-image]: https://img.shields.io/travis/feross/simple-get.svg?style=flat
[travis-url]: https://travis-ci.org/feross/simple-get
[npm-image]: https://img.shields.io/npm/v/simple-get.svg?style=flat
[npm-url]: https://npmjs.org/package/simple-get
[downloads-image]: https://img.shields.io/npm/dm/simple-get.svg?style=flat
[downloads-url]: https://npmjs.org/package/simple-get

### Simplest way to make http get requests

## features

This module is designed to be the lightest possible wrapper on top of node.js `http`, but supporting:

- follows redirects
- automatically handles gzip/deflate responses
- supports HTTPS
- supports convenience `url` key so there's no need to use `url.parse` on the url when specifying options

All this in < 100 lines of code.

## install

```
npm install simple-get
```

## usage

### simple GET request

Doesn't get easier than this:

```js
var get = require('simple-get')

get('http://example.com', function (err, res) {
  if (err) throw err
  console.log(res.statusCode) // 200
  res.pipe(process.stdout) // `res` is a stream
})
```

### even simpler GET request

If you just want the data, and don't want to deal with streams:

```js
var get = require('simple-get')

get.concat('http://example.com', function (err, data, res) {
  if (err) throw err
  console.log(res.statusCode) // 200
  console.log(data) // 'this is the server response'
})
```

### POST, PUT, PATCH, HEAD, DELETE support

For `POST`, call `get.post` or use option `{ method: 'POST' }`.

```js
var get = require('simple-get')

var opts = {
  url: 'http://example.com',
  body: 'this is the POST body'
}
get.post(opts, function (err, res) {
  if (err) throw err
  res.pipe(process.stdout) // `res` is a stream
})
```

A more complex example:

```js
var get = require('simple-get')
var concat = require('concat-stream')

get({
  url: 'http://example.com',
  method: 'POST',
  body: 'this is the POST body',

  // simple-get accepts all options that node.js `http` accepts
  // See: http://nodejs.org/api/http.html#http_http_request_options_callback
  headers: {
    'user-agent': 'my cool app'
  }
}, function (err, res) {
  if (err) throw err

  // All properties/methods from http.IncomingResponse are available,
  // even if a gunzip/inflate transform stream was returned.
  // See: http://nodejs.org/api/http.html#http_http_incomingmessage
  res.setTimeout(10000)
  console.log(res.headers)

  res.pipe(concat(function (data) {
    // `data` is the decoded response, after it's been gunzipped or inflated
    // (if applicable)
    console.log('got the response: ' + data)
  }))

})
```

## license

MIT. Copyright (c) [Feross Aboukhadijeh](http://feross.org).
