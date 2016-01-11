


`actions.resultException(req, res, err, headers, verbose)`

The function generates an error response. If @FA{verbose} is set to
*true* or not specified (the default), then the error stack trace will
be included in the error message if available. If @FA{verbose} is a string
it will be prepended before the error message and the stacktrace will also
be included.

