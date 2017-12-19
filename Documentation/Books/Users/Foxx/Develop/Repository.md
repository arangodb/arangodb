Details on FoxxRepository
=========================

A repository is a gateway to the database. It gets data from the database, updates it or saves new data. It uses the given model when it returns a model and expects instances of the model for methods like save. In your repository file, export the repository as **repository**.

```javascript
var Foxx = require("org/arangodb/foxx");

var TodosRepository = Foxx.Repository.extend({
    // ...
});

exports.repository = TodosRepository;
```

The following events are emitted by a repository:

- beforeCreate
- afterCreate
- beforeSave
- afterSave
- beforeUpdate
- afterUpdate
- beforeRemove
- afterRemove

Model lifecycle:

```js
var person = new PersonModel();
person.on('beforeCreate', function() {
    this.fancyMethod(); // Do something fancy with the model
});
var people = new Repository(appContext.collection("people"), { model: PersonModel });

people.save(person);
// beforeCreate(person)
// beforeSave(person)
// The model is created at db
// afterSave(person)
// afterCreate(person)

people.update(person, data);
// beforeUpdate(person, data)
// beforeSave(person, data)
// The model is updated at db
// afterSave(person, data)
// afterUpdate(person, data)

people.remove(person);
// beforeRemove(person)
// The model is deleted at db
// afterRemove(person)
```

### Initialize

@startDocuBlock JSF_foxx_repository_initializer

Defining custom queries
-----------------------

You can define custom query methods using Foxx.createQuery and Foxx.Repository.extend.

For more details see the chapter on [Foxx Queries](../Develop/Queries.md).

**Examples**

Making a simple query in the repository and using it from the controller:

```js
// in the repository
var Foxx = require("org/arangodb/foxx");

var TodosRepository = Foxx.Repository.extend({
    getPendingItems: Foxx.createQuery(
        'FOR todo IN my_todos FILTER todo.completed == false RETURN todo'
    )
});

// in the controller
ctrl.get("/", function(req, res) {
    req.json(todosRepository.getPendingItems());
});
```

It is also possible to supply parameters to the query:

```js
// in the repository
TodosRepository.prototype.getPendingItemById = Foxx.createQuery({
    query: 'FOR todo IN my_todos FILTER todo.completed == false FILTER todo._key == @id RETURN todo',
    params: ['id']
});

// in the controller
ctrl.get("/:id", function(req, res) {
    var id = req.params("id");
    var rv = todosRepository.getPendingItemById(id);
    res.json(rv);
});
```

The list of results can also be transformed before returning it from the repository:

```js
// in the repository
TodosRepository.prototype.getPendingItemById = Foxx.createQuery({
    query: 'FOR todo IN my_todos FILTER todo.completed == false FILTER todo._key == @id RETURN todo',
    params: ['id'],
    transform: function(results, extra) {
        for (var i = 0; i < results.length; i++) {
            results[i].extraProperty = extra;
        }
    }
});

// in the controller
ctrl.get("/:id", function(req, res) {
    var id = req.params("id");
    var extra = req.params("extra");
    var rv = todosRepository.getPendingItemById(id, extra);
    res.json(rv);
});
```

Attributes of a Repository
--------------------------

### Collection

@startDocuBlock JSF_foxx_repository_collection

### Model

@startDocuBlock JSF_foxx_repository_model

### Model schema

@startDocuBlock JSF_foxx_repository_modelSchema

### Prefix

@startDocuBlock JSF_foxx_repository_prefix

Defining indexes
----------------

Repository can take care of ensuring the existence of collection indexes for you.
If you define indexes for a repository, instances of the repository will have
access to additional index-specific methods like **range** or **fulltext** (see below).

The syntax for defining indexes is the same used in [*collection.ensureIndex*](../../IndexHandling/README.md).

**Examples**

```js
var Foxx = require('org/arangodb/foxx');
var FulltextRepository = Foxx.Repository.extend({
    indexes: [
        {
            type: 'fulltext',
            fields: ['text'],
            minLength: 3
        }
    ]
});
```

Methods of a Repository
-----------------------

### Adding entries to the repository

@startDocuBlock JSF_foxx_repository_save

### Finding entries in the repository

@startDocuBlock JSF_foxx_repository_byId

@startDocuBlock JSF_foxx_repository_byExample

@startDocuBlock JSF_foxx_repository_firstExample

@startDocuBlock JSF_foxx_repository_all

@startDocuBlock JSF_foxx_repository_any

### Removing entries from the repository

@startDocuBlock JSF_foxx_repository_remove

@startDocuBlock JSF_foxx_repository_removeById

@startDocuBlock JSF_foxx_repository_removeByExample

### Replacing entries in the repository

@startDocuBlock JSF_foxx_repository_replace

@startDocuBlock JSF_foxx_repository_replaceById

@startDocuBlock JSF_foxx_repository_replaceByExample

### Updating entries in the repository
@startDocuBlock JSF_foxx_repository_update

@startDocuBlock JSF_foxx_repository_updateById

@startDocuBlock JSF_foxx_repository_updateByExample

@startDocuBlock JSF_foxx_repository_exists

### Counting entries in the repository

@startDocuBlock JSF_foxx_repository_count

### Index-specific repository methods

@startDocuBlock JSF_foxx_repository_range

@startDocuBlock JSF_foxx_repository_near

@startDocuBlock JSF_foxx_repository_within

@startDocuBlock JSF_foxx_repository_fulltext
