


`Route.errorResponse(errorClassOrName, code, description, [callback])`

Define a reaction to a thrown error for this route: If your handler throws an error
of the errorClass defined in `errorClassOrName` or the error has an attribute `name` equal to `errorClassOrName`,
it will be caught and the response object will be filled with the given
status code and a JSON with error set to your description as the body.

If you want more control over the returned JSON, you can give an optional fourth
parameter in form of a function. It gets the error as an argument, the return
value will be transformed into JSON and then be used as the body.
The status code will be used as described above. The description will be used for
the documentation.

It also adds documentation for this error response to the generated documentation.

*Examples*

```js
/* define our own error type, FoxxyError */
var FoxxyError = function (message) {
  this.name = "FError";
  this.message = "the following FoxxyError occurred: " + message;
};
FoxxyError.prototype = new Error();

app.get("/foxx", function {
  /* throws a FoxxyError */
  throw new FoxxyError();
}).errorResponse(FoxxyError, 303, "This went completely wrong. Sorry!");

app.get("/foxx", function {
  throw new FoxxyError("oops!");
}).errorResponse("FError", 303, "This went completely wrong. Sorry!", function (e) {
  return {
    code: 123,
    desc: e.message
  };
});
```

