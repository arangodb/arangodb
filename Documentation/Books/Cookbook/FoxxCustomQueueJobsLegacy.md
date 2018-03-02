# Creating a custom job type for Foxx Queues

By combining our knowledge about [background workers](FoxxQueuesLegacy.html) and [making requests](MakingRequests.html) we can integrate with third-party web applications by writing our own Foxx Queue job types.

**Note:** For this recipe you need Arango 2.4 or 2.5. An updated version of this recipe is available [for Arango 2.6 and above](FoxxCustomQueueJobs.html).

## Problem

I have an existing Foxx app (for example the **todo app** from the [first recipe about Foxx](FoxxFirstSteps.html)) and want to send a message to my [Gitter chat](https://gitter.im) whenever an item is added.

## Solution

Go to [Gitter](https://gitter.im) and sign up for a free account there. Create a room and click on "Configure your integrations" in the side bar and select "Custom". Note down the URL (it should look something like this: `https://webhooks.gitter.im/e/b4dc0ff33b4b3`).

We may want to be able to send messages to Gitter in other apps too, so let's create a new Foxx app we call `notify-gitter`. We'll only add a single JavaScript file containing an export called `webhook` and we want to allow configuring our Foxx app with the webhook URL, so our manifest should look similar to this:

```json
{
  "name": "notify-gitter",
  "version": "1.0.0",
  "exports": {
    "webhook": "webhook.js"
  },
  "configuration": {
    "url": {
      "description": "Gitter webhook URL.",
      "type": "string",
      "default": "https://webhooks.gitter.im/e/123"
    }
  }
}
```

We'll create a new job type called `notify.gitter`. The implementation in our `webhook.js` is relatively straightforward:

```js
'use strict';
var queues = require('org/arangodb/foxx').queues;
var request = require('org/arangodb/request');

queues.registerJobType('notify.gitter', {
  execute: function (data) {
    var response = request.post(applicationContext.configuration.url, {
      form: data
    });
    if (response.status >= 400) {
      return response.body;
    } else {
      throw new Error(
        'Server returned error code ' +
        response.status +
        ' with message: ' +
        response.body
      );
    }
  }
});
```

We can now install the new Foxx app and mount it alongside our existing app. Make sure to swap out the default webhook URL with the one you received from Gitter before you proceed. Once the app has been configured correctly, we can use the new job type like any other job type available from the Foxx app registry.

First create a new queue:

```js
var queue = Foxx.queues.create('todo-gitter', 1)
```

Then use the queue in your controller:

```js
controller.post('/', function (req, res) {
  var todo = req.params('todo');
  todos.save(todo);
  res.json(todo.forClient());

  queue.push('notify.gitter', {
    message: '**New Todo added**: ' + todo.get('title')
  });
}).bodyParam('todo', {description: 'The Todo you want to create', type: Todo});
```

And that's a wrap!

**Author**: [Alan Plum](https://github.com/pluma)

**Tags**: #foxx
