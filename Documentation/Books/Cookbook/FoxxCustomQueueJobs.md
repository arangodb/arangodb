# Creating a custom job type for Foxx Queues

By combining our knowledge about [background workers](FoxxQueues.html) and [Foxx scripts](FoxxScripts.html) we can turn our scripts into our own job types for Foxx Queues.

**Note:** For this recipe you need at least Arango 2.6. An older version of this recipe is available [for Arango 2.4 and 2.5](FoxxCustomQueueJobsLegacy.html).

## Problem

I have an existing Foxx app (for example the **todo app** from the [first recipe about Foxx](FoxxFirstSteps.html)) and an existing Foxx script (for example the **Gitter webhook** from the [recipe about Foxx scripts](FoxxScripts.html)) and want to invoke the script whenever an item is added.

## Solution

A job type in ArangoDB 2.6 and above consists of at least two properties: the name of a script and the mount point of the app that provides that script. This means you can use any script provided by any app as a job type, but let's save our future selves some effort and make the Gitter app export those properties for us. First open its `manifest.json` and add an "exports" section:

```json
{
  // ...
  "exports": "exports.js"
}
```

Next create the `exports.js` file. We want developers using our app to be able to just import it and get a job type they can use directly. We know our script is called "webhook" because that's what we called it in the manifest but the mount path can not be known before the app is actually mounted. Luckily we can access an app's own mount point using `applicationContext.mount`. The code is easy enough:

```js
'use strict';
module.exports = {
  mount: applicationContext.mount,
  name: 'webhook'
};
```

Update the mounted Gitter app (or mount it if it hasn't been mounted yet) and the exports will become available. This is all you need to do to make the Gitter app available to other apps as a job type.

The next steps should be familiar: open the todo app and add the Gitter app as a dependency in its `manifest.json` file:

```json
{
  // ...
  "dependencies": {
    "gitter": "notify-gitter:^1.0.0"
  }
}
```

The dependency is now available via the alias `applicationContext.dependencies.gitter` in your code. So let's use it. First create a new queue:

```js
var queue = Foxx.queues.create('todo-gitter', 1)
```

Then use the queue in your controller:

```js
controller
.post('/', function (req, res) {
  var todo = req.params('todo');
  todos.save(todo);
  res.json(todo.forClient());

  queue.push(applicationContext.dependencies.gitter, {
    message: '**New Todo added**: ' + todo.get('title')
  });
})
.bodyParam('todo', {description: 'The Todo you want to create', type: Todo});
```

Once you've updated the mounted app from the ArangoDB web interface, find the dependencies icon in the toolbar and set the "Gitter" dependency to the mount path of the Gitter app (e.g. `/gitter`). Once the app has been configured and all of its dependencies have been set up correctly, you should now see notifications from your app in Gitter.

**Author**: [Alan Plum](https://github.com/pluma)

**Tags**: #foxx
