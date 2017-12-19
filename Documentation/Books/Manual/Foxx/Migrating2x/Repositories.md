!CHAPTER Repositories and models

Previously Foxx was heavily built around the concept of repositories and models, which provided complex but rarely necessary abstractions on top of ArangoDB collections and documents. In ArangoDB 3.0 these have been removed entirely.

!SECTION Repositories vs collections

Repositories mostly wrapped methods that already existed on ArangoDB collection objects and primarily dealt with converting between plain ArangoDB documents and Foxx model instances. In ArangoDB 3.0 you can simply use these collections directly and treat documents as plain JavaScript objects.

Old:

```js
'use strict';
const Foxx = require('org/arangodb/foxx');
const myRepo = new Foxx.Repository(
  applicationContext.collection('myCollection'),
  {model: Foxx.Model}
);

// ...

const models = myRepo.byExample({color: 'green'});
res.json(models.map(function (model) {
  return model.forClient();
}));
```

New:

```js
'use strict';
const myDocs = module.context.collection('myCollection');

// ...

const docs = myDocs.byExample({color: 'green'});
res.json(docs);
```

!SECTION Schema validation

The main purpose of models in ArangoDB 2.x was to validate incoming data using joi schemas. In more recent versions of ArangoDB 2.x it was already possible to pass these schemas directly in most places where a model was expected as an argument. The only difference is that schemas should now be considered the default.

If you previously relied on the automatic validation of Foxx model instances when setting attributes or instantiating models from untrusted data, you can simply use the schema's `validate` method directly.

Old:

```js
'use strict';
const joi = require('joi');
const mySchema = {
  name: joi.string().required(),
  size: joi.number().required()
};
const Foxx = require('org/arangodb/foxx');
const MyModel = Foxx.Model.extend({schema: mySchema});

// ...

const model = new MyModel(req.json());
if (!model.isValid) {
  res.status(400);
  res.write('Bad request');
  return;
}
```

New:

```js
'use strict';
const joi = require('joi');
// Note this is now wrapped in a joi.object()
const mySchema = joi.object({
  name: joi.string().required(),
  size: joi.number().required()
}).required();

// ...

const result = mySchema.validate(req.body);
if (result.errors) {
  res.status(400);
  res.write('Bad request');
  return;
}
```

!SECTION Migrating models

While most use cases for models can now be replaced with plain joi schemas, there is still the concept of a "model" in Foxx in ArangoDB 3.0 although it is quite different from Foxx models in ArangoDB 2.x.

A model in Foxx now refers to a plain JavaScript object with an optional `schema` attribute and the optional methods `forClient` and `fromClient`. Models can be used instead of plain joi schemas to define request and response bodies but there are no model "instances" in ArangoDB 3.0.

Old:

```js
'use strict';
const _ = require('underscore');
const joi = require('joi');
const Foxx = require('org/arangodb/foxx');
const MyModel = Foxx.Model.extend({
  schema: {
    name: joi.string().required(),
    size: joi.number().required()
  },
  forClient () {
    return _.omit(this.attributes, ['_key', '_id', '_rev']);
  }
});

// ...

ctrl.get(/* ... */)
.bodyParam('body', {type: MyModel});
```

New:

```js
'use strict';
const _ = require('lodash');
const joi = require('joi');
const MyModel = {
  schema: joi.object({
    name: joi.string().required(),
    size: joi.number().required()
  }).required(),
  forClient (data) {
    return _.omit(data, ['_key', '_id', '_rev']);
  }
};

// ...

router.get(/* ... */)
.body(MyModel);
```

!SECTION Triggers

When saving, updating, replacing or deleting models in ArangoDB 2.x using the repository methods the repository and model would fire events that could be subscribed to in order to perform side-effects.

Note that even in 2.x these events would not fire when using queries or manipulating documents in any other way than using the specific repository methods that operated on individual documents.

This behaviour is no longer available in ArangoDB 3.0 but can be emulated by using an `EventEmitter` directly if it is not possible to solve the problem differently:

Old:

```js
'use strict';
const Foxx = require('org/arangodb/foxx');
const MyModel = Foxx.Model.extend({
  // ...
}, {
  afterRemove () {
    console.log(this.get('name'), 'was removed');
  }
});

// ...

const model = myRepo.firstExample({name: 'myName'});
myRepo.remove(model);
// -> "myName was removed successfully"
```

New:

```js
'use strict';
const EventEmitter = require('events');
const emitter = new EventEmitter();
emitter.on('afterRemove', function (doc) {
  console.log(doc.name, 'was removed');
});

// ...

const doc = myDocs.firstExample({name: 'myName'});
myDocs.remove(doc);
emitter.emit('afterRemove', doc);
// -> "myName was removed successfully"
```

Or simply:

```js
'use strict';
function afterRemove(doc) {
  console.log(doc.name, 'was removed');
}

// ...

const doc = myDocs.firstExample({name: 'myName'});
myDocs.remove(doc);
afterRemove(doc);
// -> "myName was removed successfully"
```
