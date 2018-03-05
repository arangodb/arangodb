


`Controller.extend(extensions)`

Extends all functions to define routes in this controller.
This allows to combine several route extensions with the invocation
of a single function.
This is especially useful if you use the same input parameter in several routes of
your controller and want to apply the same validation, documentation and error handling
for it.

The `extensions` parameter is a JSON object with arbitrary keys.
Each key is used as the name of the function you want to define (you cannot overwrite
internal functions like `pathParam`) and the value is a function that will be invoked.
This function can get arbitrary many arguments and the `this` of the function is bound
to the route definition object (e.g. you can use `this.pathParam()`).
Your newly defined function is chainable similar to the internal functions.

**Examples**

Define a validator for a queryParameter, including documentation and errorResponses
in a single command:

```js
controller.extend({
  myParam: function (maxValue) {
    this.queryParam("value", {type: joi.number().required()});
    this.onlyIf(function(req) {
      var v = req.param("value");
      if (v > maxValue) {
        throw new NumberTooLargeError();
      }
    });
    this.errorResponse(NumberTooLargeError, 400, "The given value is too large");
  }
});

controller.get("/goose/barn", function(req, res) {
  // Will only be invoked if the request has parameter value and it is less or equal 5.
}).myParam(5);
```


