


`actions.resultOk(req, res, code, result, headers)`

The function defines a response. *code* is the status code to
return. *result* is the result object, which will be returned as JSON
object in the body. *headers* is an array of headers to returned.
The function adds the attribute *error* with value *false*
and *code* with value *code* to the *result*.

