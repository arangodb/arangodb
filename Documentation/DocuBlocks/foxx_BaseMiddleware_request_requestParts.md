


`request.requestParts()`

Returns an array containing the individual parts of a multi-part request.
Each part contains a `headers` attribute with all headers of the part,
and a `data` attribute with the content of the part in a Buffer object.
If the request is not a multi-part request, this function will throw an
error.

