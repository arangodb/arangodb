# Adding Background workers to my Foxx App

ArangoDB Foxx allows defining job queues that let you perform slow or expensive actions asynchronously. These queues can be used to send emails, call external APIs or perform other actions that you do not want to perform directly or want to retry on failure.

**Note:** For this recipe you need Arango 2.4 or 2.5. An updated version of this recipe is available [for Arango 2.6 and above](FoxxQueues.html).

## Problem

I have an existing Foxx app (for example the **todo app** from the [first recipe about Foxx](FoxxFirstSteps.html)) and I want to send out an email every time an item is created.

## Solution

Go to [SendGrid](https://sendgrid.com) and sign up for a free account there. You will need your credentials (your username and password) to configure the Foxx Job for SendGrid. First, install the job by going to the admin interface of ArangoDB and clicking on 'Applications' and then 'Add Application'. Now click on the 'Store' button and search for the `mailer-sendgrid` app from Alan Plum. Click the 'Install' button next to it.

Mount it to `/sendgrid` and provide your username and password from SendGrid. Now press configure, and the app will be installed and configured. Now, let's go to your app.

In your controller, first add a variable for a new queue with `queue = Foxx.queues.create('todo-mailer', 1)`. This will create a new queue with the name `todo-mailer` and give you access to it via the `queue` variable.

Now we want to push a job onto our queue every time somebody adds an item to the todo list (the route that posts to `/`). Here is the according code (don't forget to adjust the `to` field to be your email address):

```js
controller.post('/', function (req, res) {
  var todo = req.params('todo');
  todos.save(todo);
  res.json(todo.forClient());

  queue.push('mailer.sendgrid', {
    from: 'todos@example.com',
    to: 'YOUR ADDRESS',
    subject: 'New Todo Added',
    html: 'New Todo added: ' + todo.get('title')
  });
}).bodyParam('todo', {description: 'The Todo you want to create', type: Todo});
```

And that's it! To try it out open your todo app from the Applications tab in the ArangoDB web interface and create a new todo item with the interactive documentation. You will receive an email with the title of the todo shortly after that. As we are using the queue, the user of your API doesn't need to wait until the email is sent, but gets an immediate answer.

Read the [Foxx Queues documentation](https://docs.arangodb.com/Foxx/Develop/Queues.html) to get familiar with handling queues in your Foxx apps.

**Author**: [Lucas Dohmen](https://github.com/moonglum)

**Tags**: #foxx
