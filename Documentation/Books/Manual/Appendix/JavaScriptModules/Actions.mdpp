!CHAPTER Module "actions" 

The action module provides the infrastructure for defining low-level HTTP actions.

If you want to define HTTP endpoints in ArangoDB you should probably use the [Foxx microservice framework](../../Foxx/README.md) instead.

!SECTION Basics

!SUBSECTION Error message 

<!-- js/server/modules/@arangodb/actions.js -->

`actions.getErrorMessage(code)`

Returns the error message for an error code.

!SECTION Standard HTTP Result Generators

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

!SUBSECTION Result ok

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultOk(req, res, code, result, headers)`

The function defines a response. *code* is the status code to
return. *result* is the result object, which will be returned as JSON
object in the body. *headers* is an array of headers to returned.
The function adds the attribute *error* with value *false*
and *code* with value *code* to the *result*.

!SUBSECTION Result bad

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultBad(req, res, error-code, msg, headers)`

The function generates an error response.

!SUBSECTION Result not found

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultNotFound(req, res, code, msg, headers)`

The function generates an error response.

!SUBSECTION Result unsupported

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultUnsupported(req, res, headers)`

The function generates an error response.

!SUBSECTION Result error

<!-- js/server/modules/@arangodb/actions.js -->

*actions.resultError(*req*, *res*, *code*, *errorNum*,
                         *errorMessage*, *headers*, *keyvals)*

The function generates an error response. The response body is an array
with an attribute *errorMessage* containing the error message
*errorMessage*, *error* containing *true*, *code* containing
*code*, *errorNum* containing *errorNum*, and *errorMessage*
containing the error message *errorMessage*. *keyvals* are mixed
into the result.

!SUBSECTION Result not Implemented

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultNotImplemented(req, res, msg, headers)`

The function generates an error response.

!SUBSECTION Result permanent redirect

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultPermanentRedirect(req, res, options, headers)`

The function generates a redirect response.

!SUBSECTION Result temporary redirect

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultTemporaryRedirect(req, res, options, headers)`

The function generates a redirect response.

!SECTION ArangoDB Result Generators

!SUBSECTION Collection not found

<!-- js/server/modules/@arangodb/actions.js -->

`actions.collectionNotFound(req, res, collection, headers)`

The function generates an error response.

!SUBSECTION Index not found

<!-- js/server/modules/@arangodb/actions.js -->

`actions.indexNotFound(req, res, collection, index, headers)`

The function generates an error response.

!SUBSECTION Result exception

<!-- js/server/modules/@arangodb/actions.js -->

`actions.resultException(req, res, err, headers, verbose)`

The function generates an error response. If @FA{verbose} is set to
*true* or not specified (the default), then the error stack trace will
be included in the error message if available. If @FA{verbose} is a string
it will be prepended before the error message and the stacktrace will also
be included.
