# My first Foxx App

## Problem

I want to create a simple API with [Foxx](https://foxx.arangodb.com), but I never created one before. Specifically I want to create simple API to organize my todo items.

**Note:** For this recipe you need at least Arango 2.4. For versions before look at the [old My first Foxx App](FoxxFirstStepsLegacy.md).

## Solution

### Start up ArangoDB in development mode

Create a folder `foxx_apps` somewhere where you have write access. Create a folder `databases` inside of it, and one called `_system` inside of that. Now you can start ArangoDB with the following command:

```
arangod --javascript.dev-app-path /PATH/TO/foxx_apps /PATH/TO/DB
```

In that case, `/PATH/TO/foxx_apps` is the path to the `foxx_apps` folder you created above and `/PATH/TO/DB` is the path to the folder where your database should be. If you can see the admin interface on `http://localhost:8529` you're now ready to go. Click on 'Applications' in the navigation bar – this is the place where all your Foxx apps are. We are now ready to go.

*Don't have ArangoDB installed yet? Check out the instructions [on our downloads page](https://www.arangodb.com/download).*

### Create your app

Now in the application area click on "Add Application". Now enter the required information (without any collections) and click "Generate". This will create the app. It will prompt the path to your app to you which you should open with the text editor of your choice. In the list of apps you will now find an app with the name you chose. When you click on the app you will see a page with information about your app including the documentation.

We could also have added some collections when generating. This would have created the collections and the according CRUD actions. But in this recipe we want to see how to write them ourselves.

### Create the required collections

In ArangoDB you need to create a collection in order to save documents. We will need a collection for our todos. We need to do that before the app is running. In Foxx we use a setup script to prepare ArangoDB for running our app, which includes creating collections. Add the following line to the `scripts/setup.js` file:

```js
createCollection('todos');
```

If you want to learn more about `db`, please check out [our documentation about handling collections](https://docs.arangodb.com/Collections/index.html). The setup script will be executed when the app is installed. In the development mode however, it will be called every time we request the app. If we click on the tiny `i` on our app, we will get to interactive documentation for our app (which is empty right now, because we don't have any routes yet). This will trigger our app and therefore execute our setup script. To check if that worked, go to `collections`. You will see a collection called `dev_todos_todos`. Setup is done!

### Create our first route

In Foxx, we use controllers to add routes to our application. Inside of the folder `controller` create your first controller in a file `todos.js`:

```js
var Foxx = require('org/arangodb/foxx'),
  controller;

controller = new Foxx.Controller(applicationContext);

controller.get('/', function (req, res) {
  res.json({
    hello: 'world'
  });
});
```

This will add a `get` route on the route of the controller which returns a JSON response `{ "hello": "world" }`. We now need to mount that controller to some endpoint, this is again done in our manifest file:

```json
"controllers": {
  "/todos": "controllers/todos.js"
}
```

Our controller is now reachable on the `todos` endpoint. Want to see it in action? Let's go to the interactive documentation for our app again (click on Applications, then on the `i` next to the app `todos (dev)`). Click on `todos` and then you will see a `get` route – clicking on that will reveal the documentation for this specific route. Click `Try it out` and you will see the response we defined. But this description seems a little empty – let's add some description of what this route should do to our controller:

```js
var Foxx = require('org/arangodb/foxx'),
  controller;

controller = new Foxx.Controller(applicationContext);

/** Get a list of all todos
 *
 * Get a list of all todos that are stored in the database.
 */
controller.get('/', function (req, res) {
  res.json({
    hello: 'world'
  });
});
```

If you go to the interactive documentation again, you will now see the description you just added. Great! If you want more information about controllers, see [in the documentation](https://docs.arangodb.com/Foxx/Develop/Controller.html).

### Add a model that describes our todo items

We now need to define how we want a single todo item looks like. Foxx uses this information for both the documentation as well as for validating inputs. In a file called `todo.js` in the folder `models` you put the following [Foxx Model](https://docs.arangodb.com/Foxx/Develop/Model.html) prototype:

```js
var Foxx = require('org/arangodb/foxx'),
  Joi = require('joi'),
  Todo;

Todo = Foxx.Model.extend({
  schema: {
    completed: Joi.boolean().default(true),
    order: Joi.number().integer().required(),
    title: Joi.string().required()
  }
});

module.exports = Todo;
```

Now we can use the model in our post route to create new items. We want to store the model in the database – in order to do that we create a Repository. A repository has the responsibility to store models in a collection as well as giving a nice interface for searching for those models as well as updating or deleting them. To create a repository you have to provide the collection it should use as well as the model used for the documents in the collection. We create a standard repository in the following snippet. Our post action receives a Todo as its body. We provide a name for it as well as a short description. In the body of the function we then get the parameter `todo` which is an instance of our Todo model. We then save this model into our database. This will also add some meta data to the model (the ID, key and revision from ArangoDB). We then send a response back to the client.

```js
var Foxx = require('org/arangodb/foxx'),
  Todo = require('models/todo'),
  todosCollection = applicationContext.collection("todos"),
  todos,
  controller;

todos = new Foxx.Repository(todosCollection, {
  model: Todo
});

controller = new Foxx.Controller(applicationContext);

/** Get a list of all todos
*
* Get a list of all todos that are stored in the database.
*/
controller.get('/', function (req, res) {
  res.json({
    hello: 'world'
  });
});

/** Add a new todo to the list
*
* Create a new todo and add it to the list of todos.
*/
controller.post('/', function (req, res) {
  var todo = req.params('todo');
  todos.save(todo);
  res.json(todo.forClient());
}).bodyParam('todo', {description: 'The Todo you want to create', type: Todo});
```


We can now create a todo via the interactive documentation. If you click on the post route, you will see a text box where you can add the raw body that you want to send to the database. Enter the following for example:

```json
{
  "order": 1,
  "title": "Clean my house"
}
```

This will create a new todo and add it to the collection. If you look into the collection, you will see that the completed attribute is set to `false`, because that's what we set as a default in our model.

### Get a list of all todos

Our initial implementation of the route to get all todos was a fake, let us fix that. We will use [underscore.js](http://underscorejs.org) to iterate over all stored todos to call the `forClient` method on them. First require underscore as `_` (`_ = require('underscore')`), then change the index action to be the following:

```js
controller.get('/', function (req, res) {
  res.json(_.map(todos.all(), function (todo) {
    return todo.forClient();
  }));
});
```

Try it out in the interactive documentation, you will get a list of all the todos.

### Extending the representation of our model

In both our list as well as the return value we don't get the key of the document. In order to delete a single item we will however need this information. Let's change the `forClient` method of our model in order to get this information. The method could look like this:

```js
forClient: function () {
  return {
    completed: this.get('completed'),
    order: this.get('order'),
    title: this.get('title'),
    key: this.get('_key')
  };
}
```

We now get the key in both routes.

### Deleting a todo item

We could for example define the following route to delete a todo:

```js
controller.del('/:key', function (req, res) {
  var key = req.params('key');
  todos.removeById(key);
  res.json({ success: true });
}).pathParam('key', Joi.string().description('The key of the Todo'));
```

`pathParam` works similar to `bodyParam`, but for path parameters. Declare a path parameter in the route by prefixing it with a colon. We use joi again to describe our parameter. 

```js
controller.del('/:key', function (req, res) {
  var key = req.params('key');
  todos.removeById(key);
  res.json({ success: true });
}).pathParam('key', Joi.string().description('The key of the Todo'));
```

When you try this with a todo item you created, it will work. Send the same request again: It will blow up with an error: The document could not be found. This is of course correct, but you may want to have a nicer error response. In Foxx, we just add an error response description to the route and Foxx will handle this case for us. When ArangoDB (require it with `ArangoError = require('org/arangodb').ArangoError`) can't find a document, it will throw an 'ArangoDB error'. We will instead return a 404 status code with a descriptive error message:

```js
controller.del('/:key', function (req, res) {
  var key = req.params('key');
  todos.removeById(key);
  res.json({ success: true });
}).pathParam('key', Joi.string().description('The key of the Todo'))
  .errorResponse(ArangoError, 404, 'The todo item could not be found');
```

## Comment

* If you want to have custom methods on your repository, you can extend it in the same way you extended the Foxx.Model. [Learn more about it here](https://docs.arangodb.com/Foxx/Develop/Repository.html)
* We will add a new recipe for authentication in the future. In the mean time check out the [foxx-sessions-example](https://github.com/arangodb/foxx-sessions-example) app.
* We will also talk about workers and how to do work outside the request-response-cycle. Until then [check out the documentation for it](https://docs.arangodb.com/Foxx/Develop/Queries.html).


**Author**: [Lucas Dohmen](https://github.com/moonglum)

**Tags**: #foxx
