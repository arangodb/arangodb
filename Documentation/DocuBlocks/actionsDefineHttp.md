


`actions.defineHttp(options)`

Defines a new action. The *options* are as follows:

`options.url`

The URL, which can be used to access the action. This path might contain
slashes. Note that this action will also be called, if a url is given such that
*options.url* is a prefix of the given url and no longer definition
matches.

`options.prefix`

If *false*, then only use the action for exact matches. The default is
*true*.

`options.callback(request, response)`

The request argument contains a description of the request. A request
parameter *foo* is accessible as *request.parametrs.foo*. A request
header *bar* is accessible as *request.headers.bar*. Assume that
the action is defined for the url */foo/bar* and the request url is
*/foo/bar/hugo/egon*. Then the suffix parts *[ "hugo", "egon" ]*
are availible in *request.suffix*.

The callback must define fill the *response*.

* *response.responseCode*: the response code
* *response.contentType*: the content type of the response
* *response.body*: the body of the response

You can use the functions *ResultOk* and *ResultError* to easily
generate a response.

