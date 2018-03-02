Details on FoxxModel
====================

The model doesn't know anything about the database. It is just a representation
of the data as an JavaScript object.  You can add and overwrite the methods of
the prototype in your model prototype via the object you give to extend. In
your model file, export the model as **model**.

```js
var Foxx = require("org/arangodb/foxx");

var TodoModel = Foxx.Model.extend({
  // ...
});

exports.model = TodoModel;
```

A Foxx Model can be initialized with an object of attributes and their values.

There's also the possibility of annotation: If you extend the model with a
**schema** property, the model's attributes will be validated against it.

You can define attributes in the schema using the bundled **joi** library.
For more information on the syntax see [the official joi documentation](https://github.com/spumko/joi).

```js
var Foxx = require("org/arangodb/foxx");
var joi = require("joi");

var PersonModel = Foxx.Model.extend({
  schema: {
      name: joi.string().required(),
      age: joi.number().integer(),
      active: joi.boolean().default(true)
  }
});

exports.model = PersonModel;
```

You can also use `joi.object` schemas directly:

```js
var PersonModel = Foxx.Model.extend({
  schema: joi.object().keys({
    name: joi.string().required(),
    age: joi.number().integer(),
    active: joi.boolean().default(true)
  })
});
```

This has two effects: On the one hand it provides documentation. If you annotated
your model, you can use it in the **bodyParam** method for documentation.
On the other hand it will influence the behavior of the constructor: If you provide
an object to the constructor, it will validate its attributes and set the special
**errors** property. This is especially useful if you want to to initialize
the Model from user input. On the other hand it will set the default value for all
attributes that have not been set by hand. An example:

```js
var person = new PersonModel({
  name: "Pete",
  admin: true
});

person.attributes // => { name: "Pete", admin: true, active: true }
person.errors // => {admin: [ValidationError: value is not allowed]}
```

The following events are emitted by a model:

- beforeCreate
- afterCreate
- beforeSave
- afterSave
- beforeUpdate
- afterUpdate
- beforeRemove
- afterRemove

Equivalent events will also be emitted by the repository handling the model.

Model lifecycle:

```js
var person = new PersonModel();
person.on('beforeCreate', function() {
  var model = this;
  model.fancyMethod(); // Do something fancy with the model
});
var people = new Repository(appContext.collection("people"), { model: PersonModel });

people.save(person);
// beforeCreate()
// beforeSave()
// The model is created at db
// afterSave()
// afterCreate()

people.update(person, data);
// beforeUpdate(data)
// beforeSave(data)
// The model is updated at db
// afterSave(data)
// afterUpdate(data)

people.remove(person);
// beforeRemove()
// The model is deleted at db
// afterRemove()
```

### Extend
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_extend

### Initialize
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_initializer

### Get
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_get

### Set
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_set

### Has
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_has

### isValid
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_isvalid

### Errors
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_errors

### Attributes
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_attributes

### forDB
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_forDB

### forClient
<!-- js/server/modules/org/arangodb/foxx/model.js -->
@startDocuBlock JSF_foxx_model_forClient
