


`Route.bodyParam(paramName, options)`

Defines that this route expects a JSON body when requested and binds it to
a pseudo parameter with the name `paramName`.
The body can than be read in the the handler using `req.params(paramName)` on the request object.
In the `options` parameter you can define how a valid request body should look like.
This definition can be done in two ways, either using *joi* directly.
Accessing the body in this case will give you a JSON object.
The other way is to use a Foxx *Model*.
Accessing the body in this case will give you an instance of this Model.
For both ways an entry for the body will be added in the Documentation in ArangoDBs WebInterface.
For information about how to annotate your models, see the Model section.
All requests sending a body that does not match the validation given this way
will automatically be rejected.

You can also wrap the definition into an array, in this case this route
expects a body of type array containing arbitrary many valid objects.
Accessing the body parameter will then of course return an array of objects.

Note: The behavior of `bodyParam` changes depending on the `rootElement` option
set in the [manifest](../Develop/Manifest.md). If it is set to `true`, it is
expected that the body is an
object with a key of the same name as the `paramName` argument.
The value of this object is either a single object or in the case of a multi
element an array of objects.

*Parameter*

 * *paramName*: name of the body parameter in `req.parameters`.
 * *options*: a joi schema or an object with the following properties:
  * *description*: the documentation description of the request body.
  * *type*: the Foxx model or joi schema to use.
  * *allowInvalid* (optional): `true` if validation should be skipped. (Default: `false`)

*Examples*

```js
app.post("/foxx", function (req, res) {
  var foxxBody = req.parameters.foxxBody;
  // Do something with foxxBody
}).bodyParam("foxxBody", {
  description: "Body of the Foxx",
  type: FoxxBodyModel
});
```

Using a joi schema:

```js
app.post("/foxx", function (req, res) {
  var joiBody = req.parameters.joiBody;
  // Do something with the number
}).bodyParam("joiBody", {
  type: joi.number().integer().min(5),
  description: "A number greater than five",
  allowInvalid: false // default
});
```

Shorthand version:

```js
app.post("/foxx", function (req, res) {
  var joiBody = req.parameters.joiBody;
  // Do something with the number
}).bodyParam(
  "joiBody",
  joi.number().integer().min(5)
  .description("A number greater than five")
  .meta({allowInvalid: false}) // default
);
```

