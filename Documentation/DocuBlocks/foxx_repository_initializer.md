

`new FoxxRepository(collection, opts)`

Create a new instance of Repository.

A Foxx Repository is always initialized with a collection object. You can get
your collection object by asking your Foxx.Controller for it: the
*collection* method takes the name of the collection (and will prepend
the prefix of your application). It also takes two optional arguments:

1. Model: The prototype of a model. If you do not provide it, it will default
to Foxx.Model
2. Prefix: You can provide the prefix of the application if you need it in
your Repository (for some AQL queries probably)

If the Model has any static methods named after the lifecycle events, they
will automatically be registered as listeners to the events emitted by this
repository.

**Examples**

```js
instance = new Repository(appContext.collection("my_collection"));
 or:
instance = new Repository(appContext.collection("my_collection"), {
  model: MyModelPrototype
});
```

Example with listeners:

```js
var ValidatedModel = Model.extend({
  schema: {...}
}, {
  beforeSave(modelInstance) {
    if (!modelInstance.valid) {
      throw new Error('Refusing to save: model is not valid!');
    }
  }
});
instance = new Repository(collection, {model: ValidatedModel});
```

